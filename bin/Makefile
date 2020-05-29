# Top level makefile, the real shit is at src/Makefile

default: all

all:

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ && $(MAKE) $@
else
	cd ../ && mkdir out && cd out && cmake ../src/ && $(MAKE) $@
endif

server:

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ && $(MAKE) $@
else
	cd ../ && mkdir out && cd out && cmake ../src/ && $(MAKE) $@
endif
	cp ./sisdb ./sisdb-server

client:

ifeq (../out, $(wildcard ../out))
	cd ../out && cmake ../src/ && $(MAKE) $@
else
	cd ../ && mkdir out && cd out && cmake ../src/ && $(MAKE) $@
endif
	cp ./sisdb ./sisdb-client

clean:

	cd ../ && rm -rf out

