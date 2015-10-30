#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

struct st_file {
	int fileno;
	unsigned long offset;
	unsigned long bytes;
	int endoffile;
	char *buffer;
	int buffer_size;
};


int st_open(struct st_file *f, const char *name, int buffer_size)
{
	int a;

	a = open(name, O_RDONLY);
	if (f < 0)
		return a;
	f->buffer = malloc(buffer_size);
	if (f->buffer == NULL) {
		close(a);
		return -2;
	}
	f->buffer_size = buffer_size;
	f->endoffile = 0;
	f->fileno = a;
	f->offset = 0;
	f->bytes  = 0;
	return 1;
}

void st_close(struct st_file *f)
{
	close(f->fileno);
	free(f->buffer);
	memset(f, 0, sizeof(struct st_file));
}

/* refill: refill a struct file internal buffer
 * @f  : a pointer to a struct file
 * returns:
 *	number of char read on success
 *	0 on EOF
 *	-1 on error
 */
static int refill(struct st_file *f, char *buff)
{
	int i, size;

	fprintf(stderr, "refill bytes: %d offset:%d\n", f->bytes, f->offset);
	if (f->offset + f->bytes > f->buffer_size / 2) { /* buffer has gone too big */
		fprintf(stderr, "Moving#%s %d bytes of data\n", buff, f->bytes);
		memcpy(f->buffer, f->buffer + f->offset, f->bytes);
		f->offset = 0;
	}
	size = f->buffer_size / 2;
	i = read(f->fileno, f->buffer + f->offset + f->bytes, size);
	if (i < 0)
		return i;
	if (i == 0) {
		fprintf(stderr, "No more: offset %d bytes: %d i=%d\n", f->offset, f->bytes, i);
		f->endoffile = 1;
	}
	f->bytes += i;
	return i;
}

static void discard_bytes(struct st_file *f)
{
	int len, i;
	char *p, *t;
	int discarded = 0;

	while (1) {
		/* refill buffer until we find a newline */
		i = refill(f, NULL);
		p = f->buffer + f->offset;
		t = memchr(p, '\n', f->bytes);
		if (t != NULL || f->endoffile) {
			len = t - p;
			discarded += len;
			fprintf(stderr, "Trunc discarding %d chars\n", discarded);
			f->bytes  -= (len + 1);
			f->offset += (len + 1);
			break;
		}
		discarded += f->bytes;
		f->offset += f->bytes;
		f->bytes = 0;
	}
}

/* read one line from a file
 * @f      : struct file
 * @buffer : where to store data read
 * @size   : read at most size char on each line
 * returns:
 *	number of char written in buff (strlen(buff) + 1)
 *	0 on EOF or error
 */
int my_read(struct st_file *f, char *buff, size_t size)
{
	int i, len;
	char *p, *t;

	size--; /* for NUL char */
	p = f->buffer + f->offset;
	t = memchr(p, '\n', f->bytes);
	/* if we found a newline within 'size' char lets return */ 
	if (t != NULL) {
		//fprintf(stderr, "GOOD1: len = %d offset=%d bytes=%d\n", t - p, f->offset, f->bytes);
		if (t - p < size) {
			len = t - p;
			memcpy(buff, p, len);
			buff[len]  = '\0';
			f->bytes  -= (len + 1);
			f->offset += (len + 1);
			return len + 1;
		} else {
			/* discarding bytes */
			memcpy(buff, p, size);
			buff[size] = '\0';
			len = t - p;
			f->bytes  -= (len + 1);
			f->offset += (len + 1);
			fprintf(stderr, "Trunc '%s' discarding %d chars\n", buff, len - size);
			return size + 1;
		}
	}
	/* we are sure there is no newline for up to f->bytes */
	if (f->bytes < size) { /** need to refill **/
		i = refill(f, "tst");
		if (i == 0)
			size = f->bytes;
	} else {
		/* no newline found for a FBB (fucking Big Buffer), suspicious */
		memcpy(buff, p, size);
		f->offset += size;
		f->bytes  -= size;
		buff[size] = '\0';
		discard_bytes(f);
		return size + 1;
	}
	p = f->buffer + f->offset;
	t = memchr(p, '\n', f->bytes);
	if (t != NULL) {
		if (t - p < size) {
			len = t - p;
			memcpy(buff, p, len);
			buff[len] = '\0';
			len++; /* return strlen(buff) + 1 */
			f->bytes  -= len;
			f->offset += len;
			return len;
		} else {
			memcpy(buff, p, size);
			buff[size] = '\0';
			len = t - p;
			f->bytes  -= (len + 1);
			f->offset += (len + 1);
			fprintf(stderr, "Trunc '%s' discarding %d chars\n", buff, len - size);
			return size + 1;
		}
	}
	memcpy(buff, p, size);
	buff[size] = '\0';
	f->offset += size;
	f->bytes  -= size;
	if (f->endoffile)
		return size;
	discard_bytes(f);
	return size + 1;
}


int main(int argc, char **argv)
{
	char buf[64];
	int f;
	struct st_file sf;

	f = st_open(&sf, argv[1], 4096);
	if (f < 0)
		exit(1);
	while (my_read(&sf, buf, sizeof(buf)))
		printf("%s\n", buf);
}
