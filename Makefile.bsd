all: subnet-tools

subnet-tools: src/*.c src/*.h
	cd src; make subnet-tools
	mv src/subnet-tools .

test-printf:
	cd src; make test-printf
	mv src/test-printf .

test:
	cd src; make test
