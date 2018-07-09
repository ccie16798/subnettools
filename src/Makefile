CC=cc
CFLAGS= -Wall -g
CFLAGS2= -O3
EXEC=subnet-tools


OBJS =  subnet_tool.o debug.o iptools.o string2ip.o bitmap.o routetocsv.o utils.o heap.o generic_csv.o \
		prog-main.o generic_command.o config_file.o st_printf.o ipinfo.o st_scanf.o st_object.o \
		bgp_tool.o generic_expr.o st_routes_csv.o ipam.o st_memory.o st_routes.o st_ea.o \
		st_help.o st_readline.o st_limits.o st_list.o st_hashtab.o st_stats.o


all: $(EXEC)

%.o: %.c %.h st_options.h
	$(CC) -c -o $@ $< $(CFLAGS)

st_scanf_ci.o: st_scanf.c st_scanf.h st_options.h
	$(CC) -c -o st_scanf_ci.o st_scanf.c $(CFLAGS) -DCASE_INSENSITIVE

subnet-tools: $(OBJS) st_scanf_ci.o
	$(CC) -o $@ $^ $(CFLAGS)

test-printf: test-printf.o debug.o utils.o st_printf.o iptools.o bitmap.o st_object.o st_memory.o string2ip.o
	$(CC) -o $@ $^ $(CFLAGS)

test : generic_csv.o debug.o utils.o
	$(CC) -o $@ $^ $(CFLAGS) -DGENERICCSV_TEST

test-read: st_readline.c
	$(CC) -o $@ $^ $(CFLAGS) -D TEST_READ

test-hash: st_hashtab.c debug.c st_memory.c st_list.c utils.c
	$(CC) -o $@ $^ $(CFLAGS) -DTEST_HASH
