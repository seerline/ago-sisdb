# Top level makefile, the real shit is at src/Makefile

default: all

all:

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ -DMODE=EXE && $(MAKE) $@
else
	cd ../ && mkdir out && cd out && cmake ../src/ -DMODE=EXE && $(MAKE) $@
endif

test:

	rm -f test

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ -DMODE=TEST && $(MAKE) 
else
	cd ../ && mkdir out && cd out && cmake ../src/ -MODE=TEST && $(MAKE) 
endif

server:

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ -DMODE=EXE && $(MAKE) $@
else
	cd ../ && mkdir out && cd out && cmake ../src/ -DMODE=EXE && $(MAKE) $@
endif
	cp ./sisdb ./sisdb-server

client:

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ -DMODE=EXE && $(MAKE) $@
else
	cd ../ && mkdir out && cd out && cmake ../src/ -DMODE=EXE && $(MAKE) $@
endif
	cp ./sisdb ./sisdb-client

clean:

	cd ../ && rm -rf out

