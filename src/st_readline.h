#ifndef ST_READLINE_H
#define ST_READLINE_H

struct st_file {
	int fileno;
	unsigned long bytes;
	int endoffile;
	int need_discard;
	char *buffer;
	char *bp; /* curr pointer */
	int buffer_size;
};


/* st_open: open a file R/O
 * @name : name of the file; if NULL, open stdin
 * @buffer_size : size of internal buffer
 * returns:
 *	pointer to malloc struct on SUCCESS
 *	NULL on error (cannot access file, or malloc failure)
 */
struct st_file *st_open(const char *name, int buffer_size);

/* st_open: release resources attached to a st_file
 * @f    : a pointer to a struct st_file
 */
void st_close(struct st_file *f);

/* st_getline_truncate: read one line from a file, truncate if line too long
 * if strlen(line) > size, chars are DISCARDED until a NEWLINE is found
 * returns a pointer to an internal buffer
 * @f         : struct file
 * @size      : read at most size char on each line; must be < buffer_size / 4
 * @read      : pointer to the number of char read (equals to strlen(s) + 1)
 * @discarded : number of discarded chars (not always precise)
 * returns:
 *	pointer to the line
 *	NULL on error or EOF
 */
char *st_getline_truncate(struct st_file *f, size_t size, int *read, int *discarded);

/* st_gets_truncate: read one line from a file, truncate if line too long
 * if strlen(line) > size, chars are DISCARDED until a NEWLINE is found
 * As opposed to st_getline_truncate, store line in a user provided buffer
 * @f         : struct file
 * @buffer    : where to store the data
 * @size      : read at most size char on each line (sizeof(buffer)), must be < buffer_size / 4
 * @read      : pointer to the number of char read (equals to strlen(s) + 1)
 * @discarded : number of discarded chars (not always precise)
 * returns:
 *	pointer to the line
 *	NULL on error or EOF
 */
char *st_gets_truncate(struct st_file *f, char *buffer, size_t size,
		int *read, int *discarded);
#else
#endif
