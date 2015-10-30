#include <stdio.h>
#include <string.h>

char buffer[4*1024];

struct st_file {
	int fileno;
	unsigned long offset;
	unsigned long bytes;
	int endoffile;
	char *buffer;
};


static int refill(struct st_file *f)
{
	int i;

	fprintf(stderr, "refill bytes: %d offset:%d\n", f->bytes, f->offset);
	if (f->offset + f->bytes + sizeof(buffer)/2 > sizeof(buffer)) {
		fprintf(stderr, "Moving %d bytes of data\n", f->bytes);
		memcpy(f->buffer, f->buffer + f->offset, f->bytes);
		f->offset = 0;
	}
	i = read(f->fileno, f->buffer + f->offset + f->bytes, sizeof(buffer) / 2);
	if (i < 0)
		return i;
	if (i == 0) {
		fprintf(stderr, "No more: offset %d bytes: %d i=%d\n", f->offset, f->bytes,i);
		f->endoffile = 1;
	}
	f->bytes += i;
	return i;
}

int my_read(struct st_file *f, char *buff, size_t size)
{
	int i, len;
	int end = 0;
	char *p, *t = NULL;
	int orig_size;

	size--; /* for NUL char */
	orig_size = size;

	p = f->buffer + f->offset;
	t = memchr(p, '\n', f->bytes);
	/* if we found a newline within 'size' char lets return */ 
	if (t != NULL) {
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
			f->offset += len + 1;
			fprintf(stderr, "Trunc#1 discarding %d chars\n", len - size);
			return size + 1;
		}
	}
	if (f->bytes < size) { /** need to refill **/
		 i = refill(f);
		 if (i == 0)
			size = f->bytes;
	} else {
		/* no newline found for a FBB (fucking Big Buffer), suspicious */
		memcpy(buff, p, size);
		f->offset += size;
		f->bytes -= size;
		buff[size] = '\0';
		return size + 1;

	}
	p = buffer + f->offset;
	t = memchr(p, '\n', size);
	if (t != NULL) {
		len = t - p;
		memcpy(buff, p, len);
		buff[len] = '\0';
		f->bytes -= (len + 1);
		f->offset += len + 1;
		return len + 1;
	}
	memcpy(buff, p, size);
	buff[size] = '\0';
	f->offset += size;
	f->bytes -= size;
	if (f->endoffile)
		return size;
	fprintf(stderr, "Trunc #2\n" );
	while (1) {
		/* refill buffer until we find a newline */
		i = refill(f);
		p = f->buffer + f->offset;
		t = memchr(p, '\n', f->bytes);
		if (t != NULL || f->endoffile ) {
			fprintf(stderr, "Trunc#2 discarding %d chars\n", len - size);
			len = t - p;
			f->bytes -= (len + 1);
			f->offset += len + 1;
			break;
		}
		f->offset += f->bytes;
		f->bytes = 0;
	}
	return size + 1;
}


int main(int argc, char **argv)
{
	char buf[512];
	int f;
	struct st_file sf;

	memset(&sf, 0, sizeof(sf));
	sf.fileno = open(argv[1], 0);
	sf.buffer = buffer;
	if (sf.fileno < 0)
		exit(1);
	while (my_read(&sf, buf, sizeof(buf))) {
		printf("%s\n", buf);
	} 

}
