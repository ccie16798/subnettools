CC=gcc
# Hey!, I am comment number 2. I want to say that CFLAGS will be the
# options I'll pass to the compiler.
CFLAGS= -Wall -g
CFLAGS2= -O3
EXEC=subnet-tools


OBJS =  subnet_tool.o debug.o iptools.o bitmap.o routetocsv.o utils.o heap.o generic_csv.o prog-main.o generic_command.o config_file.o st_printf.o ipinfo.o

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)
all: $(EXEC)

subnet-tools: $(OBJS)
	 $(CC) -o $@ $^ $(CFLAGS)

test : generic_csv.o debug.o
	 $(CC) -o $@ $^ $(CFLAGS) -D  GENERICCSV_TEST
