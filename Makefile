# Top level makefile, the real shit is at src/Makefile

default: all

#.DEFAULT:
all:
	cd src/stsdb && $(MAKE) $@

	cd src/digger && $(MAKE) $@

	cd src/ds && $(MAKE) $@

stsdb:
	cd src/stsdb && $(MAKE)

digger:
	cd src/digger && $(MAKE)

ds:
	cd src/ds && $(MAKE)

clean:

	cd src/stsdb && $(MAKE) $@

	cd src/digger && $(MAKE) $@

	cd src/ds && $(MAKE) $@
