#ifndef ST_READLINE_H
#define ST_READLINE_H

struct st_file {
	int fileno;
	unsigned long offset;
	unsigned long bytes;
	int endoffile;
	char *buffer;
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

/* read_line_truncate! read one line from a file
 * if strlen(line) > size, chars are DISCARDED
 * @f      : struct file
 * @buffer : where to store data read
 * @size   : read at most size char on each line
 * returns:
 *	number of char written in buff (strlen(buff) + 1)
 *	0 on EOF or error
 */
int read_line_truncate(struct st_file *f, char *buff, size_t size);
#else
#endif
