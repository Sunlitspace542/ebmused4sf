all: release

release:
	$(MAKE) -C src/

debug:
	$(MAKE) -C src/ debug

clean:
	rm -rf build/