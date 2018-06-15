# Top level makefile, the real shit is at src/Makefile

default: all

#.DEFAULT:
all:
	cd src/stsdb && $(MAKE) $@

stsdb:

	cd src/stsdb && $(MAKE)

clean:

	cd src/stsdb && $(MAKE) $@
