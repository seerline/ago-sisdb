# Top level makefile, the real shit is at src/Makefile

default: all

#.DEFAULT:
all:
	cd src/stsdb && $(MAKE) $@

	cd src/digger && $(MAKE) $@

stsdb:
	cd src/stsdb && $(MAKE)

digger:
	cd src/digger && $(MAKE)

clean:

	cd src/stsdb && $(MAKE) $@

	cd src/digger && $(MAKE) $@
