#ifndef ST_READLINE_H
#define ST_READLINE_H

struct st_file {
	int fileno;
	unsigned long offset;
	unsigned long bytes;
	int endoffile;
	int need_discard;
	char *buffer;
	char *bp; /* curr pointer */
	int buffer_size;
};


/* st_open: open a file R/O
 * @f    : a pointer to a struct st_file
 * @name : name of the file
 * @buffer_size : size of internal buffer
 * returns:
 *	fileno on SUCCESS
 *	Negative on error (cannot access file, or malloc failure)
 */
int st_open(struct st_file *f, const char *name, int buffer_size);

/* st_open: release resource attached to a st_file
 * @f    : a pointer to a struct st_file
 */
void st_close(struct st_file *f);

/* st_getline_truncate: read one line from a file
 * if strlen(line) > size, chars are DISCARDED
 * @f         : struct file
 * @size      : read at most size char on each line
 * @read      : pointer to the number of char read (strlen(s) + 1)
 * @discarded : number of discarded chars (if any)
 * returns:
 *	pointer to the line
 *	NULL on error or EOF
 */
char *st_getline_truncate(struct st_file *f, size_t size, int *read, int *discarded);
#else
#endif
