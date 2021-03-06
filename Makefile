all: subnet-tools

subnet-tools: src/*.c src/*.h
	make -C src
	mv src/subnet-tools .

test-printf: src/*.c src/*.h
	cd src; make test-printf
	mv src/test-printf .

test: src/*.c src/*.h
	cd src; make test

test-read: src/st_readline.c src/st_readline.h
	cd src; make test-read
	mv src/test-read .

regtest: src/*.c src/*.h
	cd regtest; sh ./regtest.sh

scantest:
	cd regtest; sh ./scanf_test.sh

test-hash: src/st_hashtab.c
	cd src; make test-hash
	mv src/test-hash .
