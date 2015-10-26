#ifndef GENERIC_COMMAND_H
#define GENERIC_COMMAND_H

struct st_command {
	char *name;
	int (*run_cmd)(int argc, char **argv, void *options);
	int required_args;
	int hidden;
};

/* those struct MUST be filled in the main program */
extern struct st_command commands[];
extern struct st_command options[];

/* generic command line parser
 * command line syntax for software using this library is :
 * $MY_PROG -opt1 @opt1_args -opt2 @opt2_args ... -optN @optN_args COMMAND @command_args
 *
 *  the '.c' file where 'main' is located should define 2 arrays :
 *
 int run_command1(int argc, char **argv, void *options) { do_whatever1(options);}
 int run_command2(int argc, char **argv, void *options) { do_whatever2(options);}

 struct st_command commands[] = {
	{ "command1", &run_command1, num_args},
	{ "command2", &run_command2, num_args},
	{ NULL, NULL, 0}  ==> ARRAY MUST END with this
 };

 int run_option1(int argc, char **argv, void *options) { update_whatever1(options);}
 int run_option2(int argc, char **argv, void *options) { update_whatever2(options);}
 struct st_command options[] = {
	{"-V", &run_option1, 0},
	{"-D", &run_option2, 1},
	{ NULL, NULL, 0}  ==> MUST END with this
 };

 Then main(int argc, char **argv)  shoud call :

 res = generic_parse_options(argc, argv, PROG_NAME, &nof);
 if (res < 0)
	exit(1);
 argc = argc - res;
 argv = argv + res;
 DO_WHATEVER_WITH(options);
 res = generic_command_run(argc, argv, PROG_NAME, &nof);
 if (res < 0)
	exit(res)
 exit(0);
*/

int generic_command_run(int argc, char **argv, char *progname, void *options);
int generic_parse_options(int argc, char **argv, char *progname, void *opt);

#else
#endif
