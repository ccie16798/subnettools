all: subnet-tools

subnet-tools:
	cd src; make
	mv src/subnet-tools .

test-printf:
	cd src; make test-printf
	mv src/test-printf .

test:
	cd src; make test
