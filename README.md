## EarthBound Music Editor (but for Star Fox)
This is a little mod to enable EBMusEd to import/export Star Fox compatible files.
The exported SONG_DATA files must then be assembled into SGSOUNDx.BIN files before assembling the game.

Thank you to all of the contributors of Earthbound Music Editor, you have enabled us to make custom Star Fox music!

Compiles on Windows with MSYS2. (I have not tested standalone MinGW or Visual Studio.)  
To compile on Linux you need Wine and `wine64-tools` installed for `winegcc` and `wrc`.  

## Original README
## EarthBound Music Editor
EBMusEd is a ROM hacking tool for editing and playing back EarthBound's N-SPC sequence data.

For a tutorial on how to use it, check out the collection of EarthBound hacking-related tutorials on the [CoilSnake Wiki](https://github.com/pk-hack/CoilSnake/wiki/EBMusEd), as well as the `Code list` reference in the Help menu.

## Background
EBMusEd was originally written by Goplat, who publicly posted the source code on [starmen.net](https://forum.starmen.net/forum)'s PK Hack subforum.

Goplat has kindly provided the source for the community to continue working on it together.

## Building with Visual Studio Code in Windows
- Have MinGW installed.
- Open the `/ebmused` directory in VS Code and press CTRL+Shift+B.
- The executable will be build to `/build/release/ebmused.exe`
