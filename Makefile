# Top level makefile, the real shit is at src/Makefile

default: all

all:

	cd ./src/sisdb && cmake . && $(MAKE) $@ 

sisdb:

	cd ./src/sisdb && cmake . && $(MAKE)
	cp ./bin/libsisdb.* ../bin
	
test:

	cd ./test && cmake . && $(MAKE)


clean:

	cd ./src/sisdb && rm -f CMakeCache.txt && rm -rf CMakeFiles

	cd ./test && rm -f CMakeCache.txt && rm -rf CMakeFiles
