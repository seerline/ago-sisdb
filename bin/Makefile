# Top level makefile, the real shit is at src/Makefile

op := out
ifeq ($(shell uname), Darwin) 
	op := mac
	@echo "y => $(op)"
endif

default: all


all:

ifeq (../$(op), $(wildcard ../$(op)))
	cd ../$(op) && cmake ../src/ -DMODE=EXE && $(MAKE) $@
else
	cd ../ && mkdir $(op) && cd $(op) && cmake ../src/ -DMODE=EXE && $(MAKE) $@
endif

test:
ifeq (../$(op), $(wildcard ../$(op)))
	cd ../$(op) && cmake ../src/ -DMODE=TEST && $(MAKE) 
else
	cd ../ && mkdir $(op) && cd $(op) && cmake ../src/ -MODE=TEST && $(MAKE) 
endif

server:

ifeq (../$(op), $(wildcard ../$(op)))
	cd ../$(op) && cmake ../src/ -DMODE=EXE && $(MAKE) $@
else
	cd ../ && mkdir $(op) && cd $(op) && cmake ../src/ -DMODE=EXE && $(MAKE) $@
endif
	cp ./sisdb ./sisdb-server

client:

ifeq (../$(op), $(wildcard ../$(op)))
	cd ../$(op) && cmake ../src/ -DMODE=EXE && $(MAKE) $@
else
	cd ../ && mkdir $(op) && cd $(op) && cmake ../src/ -DMODE=EXE && $(MAKE) $@
endif
	cp ./sisdb ./sisdb-client

clean:

	cd ../ && rm -rf $(op)

