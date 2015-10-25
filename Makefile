all: subnet-tools

subnet-tools: src/*.c src/*.h
	make -C src
	mv src/subnet-tools .

test-printf: src/*.c src/*.h
	cd src; make test-printf
	mv src/test-printf .

test: src/*.c src/*.h
	cd src; make test

regtest: src/*.c src/*.h
	cd regtest; sh ./regtest.sh
