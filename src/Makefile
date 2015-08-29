CC=cc
CFLAGS= -Wall -g
CFLAGS2= -O3
EXEC=subnet-tools


OBJS =  subnet_tool.o debug.o iptools.o bitmap.o routetocsv.o utils.o heap.o generic_csv.o \
		prog-main.o generic_command.o config_file.o st_printf.o ipinfo.o st_scanf.o st_object.o \
		bgp_tool.o generic_expr.o st_handle_csv_files.o ipam.o st_memory.o

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)
all: $(EXEC)

subnet-tools: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

test-printf: test-printf.o debug.o utils.o st_printf.o iptools.o bitmap.o st_object.o
	$(CC) -o $@ $^ $(CFLAGS)

test : generic_csv.o debug.o utils.o
	$(CC) -o $@ $^ $(CFLAGS) -DGENERICCSV_TEST
