all: release

release:
	$(MAKE) -C src/ release

debug:
	$(MAKE) -C src/ debug

clean:
	rm -rf build/