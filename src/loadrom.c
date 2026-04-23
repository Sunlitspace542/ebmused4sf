#ifndef LINUX
#include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ebmusv2.h"
#include "misc.h"
#include "id.h"

FILE *rom;
int rom_size;
int rom_offset;
int song_pointer_table_offset;
char *rom_filename;

unsigned char pack_used[NUM_SONGS][3];
unsigned short song_address[NUM_SONGS];
struct pack rom_packs[NUM_PACKS];
struct pack inmem_packs[NUM_PACKS];

static char *skip_dirname(char *filename) {
	for (char *p = filename; *p; p++)
		if (*p == '/' || *p == '\\') filename = p + 1;
	return filename;
}

static DWORD crc_table[256];

static void init_crc() {
	for (int i = 0; i < 256; i++) {
		DWORD crc = i;
		for (int j = 8; j; j--)
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc = (crc >> 1);
		crc_table[i] = crc;
	}
}

static DWORD update_crc(DWORD crc, BYTE *block, int size) {
	do {
		crc = (crc >> 8) ^ crc_table[(crc ^ *block++) & 0xFF];
	} while (--size);
	return crc;
}

static const BYTE rom_menu_cmds[] = {
	ID_SAVE_ALL, ID_CLOSE, 0
};

BOOL close_rom() {
	if (rom) {
		save_cur_song_to_pack();
		int unsaved_packs = 0;
		for (int i = 0; i < NUM_PACKS; i++)
			if (inmem_packs[i].status & IPACK_CHANGED)
				unsaved_packs++;
		if (unsaved_packs) {

			char buf[70];
			if (unsaved_packs == 1)
				sprintf(buf, "A pack has unsaved changes.\nDo you want to save?");
			else
				sprintf(buf, "%d packs have unsaved changes.\nDo you want to save?", unsaved_packs);

			int action = MessageBox2(buf, "Close", MB_ICONEXCLAMATION | MB_YESNOCANCEL);
			if (action == IDCANCEL || (action == IDYES && !save_all_packs()))
				return FALSE;
		}
		save_metadata();

		fclose(rom);
		rom = NULL;
		free(rom_filename);
		rom_filename = NULL;
		enable_menu_items(rom_menu_cmds, MF_GRAYED);
		free(areas);
		free_metadata();
		for (int i = 0; i < NUM_PACKS; i++) {
			free(rom_packs[i].blocks);
			if (inmem_packs[i].status & IPACK_INMEM)
				free_pack(&inmem_packs[i]);
		}
	}

	// Closing an SPC should be correlated with closing a ROM.
	// So whether a ROM was loaded or not, we need to reset the playback state.
	// This protects from crashes if an SPC was playing.
	free_samples();
	free_song(&cur_song);
	stop_playing();
	initialize_state();

	memset(packs_loaded, 0xFF, 3);
	current_block = -1;

	return TRUE;
}

BOOL open_rom(char *filename, BOOL readonly) {
	FILE *f = fopen(filename, readonly ? "rb" : "r+b");
	if (!f) {
		MessageBox2(strerror(errno), "Can't open file", MB_ICONEXCLAMATION);
		return FALSE;
	}

	if (!close_rom())
		return FALSE;

	fseek(f, 0, SEEK_END);
	rom_size = ftell(f);
	fseek(f, 0, SEEK_SET);
	rom_offset = rom_size & 0x200;
	if (rom_size < 0x300000) {
		MessageBox2("An EarthBound ROM must be at least 3 MB", "Can't open file", MB_ICONEXCLAMATION);
		fclose(f);
		return FALSE;
	}
	rom = f;
	rom_filename = strdup(filename);
	enable_menu_items(rom_menu_cmds, MF_ENABLED);

	init_areas();
	change_range(0xBFFE00 + rom_offset, 0xBFFC00 + rom_offset + rom_size, AREA_NOT_IN_FILE, AREA_NON_SPC);

	char *bfile = skip_dirname(filename);
	char *title = malloc(sizeof("EarthBound Music Editor") + 3 + strlen(bfile));
	sprintf(title, "%s - %s", bfile, "EarthBound Music Editor");
	SetWindowText(hwndMain, title);
	free(title);

	fseek(f, BGM_PACK_TABLE + rom_offset, SEEK_SET);
	fread(pack_used, NUM_SONGS, 3, f);
	// pack pointer table follows immediately after
	for (int i = 0; i < NUM_PACKS; i++) {
		int addr = fgetc(f) << 16;
		addr |= fgetw(f);
		rom_packs[i].start_address = addr;
	}

	song_pointer_table_offset = 0;
	init_crc();
	for (int i = 0; i < NUM_PACKS; i++) {
		int size;
		int count = 0;
		struct block *blocks = NULL;
		BOOL valid = TRUE;
		struct pack *rp = &rom_packs[i];

		int offset = rp->start_address - 0xC00000 + rom_offset;
		if (offset < rom_offset || offset >= rom_size) {
			valid = FALSE;
			goto bad_pointer;
		}

		fseek(f, offset, SEEK_SET);
		DWORD crc = ~0;
		while ((size = fgetw(f)) > 0) {
			int spc_addr = fgetw(f);
			if (spc_addr + size > 0x10000) { valid = FALSE; break; }
			offset += 4 + size;
			if (offset > rom_size) { valid = FALSE; break; }

			count++;
			blocks = realloc(blocks, sizeof(struct block) * count);
			blocks[count-1].size = size;
			blocks[count-1].spc_address = spc_addr;

			if (spc_addr == 0x0500) {
				int back = ftell(f);
				song_pointer_table_offset = back + 0x2E4A - 0x500;
			}

			fread(&spc[spc_addr], size, 1, f);
			crc = update_crc(crc, (BYTE *)&size, 2);
			crc = update_crc(crc, (BYTE *)&spc_addr, 2);
			crc = update_crc(crc, &spc[spc_addr], size);
		}
		crc = ~update_crc(crc, (BYTE *)&size, 2);
bad_pointer:
		change_range(rp->start_address, offset + 2 + 0xC00000 - rom_offset,
			AREA_NON_SPC, i);
		rp->status = valid ? crc != pack_orig_crc[i] : 2;
		rp->block_count = count;
		rp->blocks = blocks;
		inmem_packs[i].status = 0;
	}
	load_metadata();
	if (song_pointer_table_offset) {
		fseek(f, song_pointer_table_offset, SEEK_SET);
		fread(song_address, NUM_SONGS, 2, f);
	} else {
		close_rom();
		MessageBox2("Unable to determine location of song pointer table.", "Can't open file", MB_ICONEXCLAMATION);
		return FALSE;
	}
	return TRUE;
}

// SBN pack loading - parses SBN blocks and loads instrument data
void load_sbn_pack(BYTE *pack_data, int pack_size, int sample_dir_block, int sample_data_block, int inst_table_block, BOOL load_inst_table) {
	// Clear any existing samples
	free_samples();
	
	// Set default addresses based on standard layout
	sample_ptr_base = 0x3C00;
	inst_base = 0x3D00;
	
	// Parse SBN blocks from pack_data
	int offset = 0;
	int block_num = 0;
	while (offset < pack_size - 4) {  // Need at least 4 bytes for block header + end marker
		WORD block_size = pack_data[offset] | (pack_data[offset + 1] << 8);
		WORD block_addr = pack_data[offset + 2] | (pack_data[offset + 3] << 8);
		
		// Check for end marker
		if (block_size == 0 && block_addr == 0) break;
		
		offset += 4;  // Skip header
		
		// Process blocks by block number instead of address matching
		if (block_num == sample_dir_block) {
			// This is the sample directory (pointers to BRR samples)
			if (block_size <= 0x200 && block_addr + block_size <= 0x10000) {  // Reasonable limit
				memcpy(&spc[block_addr], &pack_data[offset], block_size);
				printf("Loaded sample directory (block %d) at 0x%04X (%d bytes)\n", block_num, block_addr, block_size);
			}
		} else if (block_num == sample_data_block) {
			// This is the sample data (BRR samples)
			if (block_addr + block_size <= 0x10000) {
				memcpy(&spc[block_addr], &pack_data[offset], block_size);
				printf("Loaded sample data (block %d) at 0x%04X (%d bytes)\n", block_num, block_addr, block_size);
			}
		} else if (load_inst_table && block_num == inst_table_block) {
			// This is the instrument table (ADSR, gain, tuning) - optional
			if (block_size <= 0x300 && block_addr + block_size <= 0x10000) {  // Reasonable limit for 128 instruments
				memcpy(&spc[block_addr], &pack_data[offset], block_size);
				printf("Loaded instrument table (block %d) at 0x%04X (%d bytes)\n", block_num, block_addr, block_size);
			}
		} else {
			// For other blocks, still load them to support chaining of SBN files
			// with different data sections (e.g., sample directory in one file, samples in another)
			if (block_addr + block_size <= 0x10000) {
				memcpy(&spc[block_addr], &pack_data[offset], block_size);
				printf("Loaded data block %d at 0x%04X (%d bytes)\n", block_num, block_addr, block_size);
			}
		}
		
		offset += block_size;
		block_num++;
	}
	
	// Decode samples from the loaded data
	decode_samples(&spc[sample_ptr_base]);
	
	// Mark as imported (not from ROM)
	spcImported = 1;
	
	printf("SBN pack loaded: sample_dir_block=%d, sample_data_block=%d, inst_table_block=%d (load=%d)\n", 
		   sample_dir_block, sample_data_block, inst_table_block, load_inst_table);
}
