#include <stdlib.h>
#include <string.h>
#include "fifo.h"

imq_fifo_t *imq_alloc_fifo(void) {
	imq_fifo_t *fifo;

	fifo = (imq_fifo_t *)malloc(sizeof (*fifo));

	fifo->data = NULL;
	fifo->size = 0;

	return fifo;
}

void imq_free_fifo(imq_fifo_t *fifo) {
	if (fifo == NULL)
		return;

	free(fifo->data);
	free(fifo);
}

int imq_splice_fifo(imq_fifo_t *fifo, int fd, imq_fifo_direction_t direction) {
	char buffer[1024];
	int rc;

	if (direction == FIFO_FROM_FD) {
		rc = read(fd, buffer, sizeof (buffer));

		if (rc <= 0)
			return rc;

		return imq_write_fifo(fifo, buffer, rc);
	} else {
		rc = imq_peak_fifo(fifo, buffer, sizeof (buffer), 0);

		if (rc <= 0)
			return rc;

		rc = write(fd, buffer, rc);

		if (rc <= 0)
			return rc;

		return imq_read_fifo(fifo, NULL, rc);
	}
}

int imq_write_fifo(imq_fifo_t *fifo, const void *data, size_t size) {
	void *new_buffer;

	if (size == 0)
		return 0;

	new_buffer = realloc(fifo->data, fifo->size + size);

	if (new_buffer == NULL)
		return -1;

	memcpy((char *)new_buffer + fifo->size, data, size);

	fifo->data = new_buffer;
	fifo->size += size;

	return size;
}

int imq_read_fifo(imq_fifo_t *fifo, void *data, size_t size) {
	int rc;
	void *new_buffer;

	rc = imq_peak_fifo(fifo, data, size, 0);

	if (rc <= 0)
		return rc;

	new_buffer = malloc(fifo->size - rc);

	memcpy(new_buffer, (char *)fifo->data + rc, fifo->size - rc);

	free(fifo->data);
	fifo->data = new_buffer;
	fifo->size -= rc;

	return rc;
}

int imq_peak_fifo(imq_fifo_t *fifo, void *data, size_t size, size_t offset) {
	if (offset > fifo->size)
		return 0;

	if (offset + size > fifo->size)
		size -= (offset + size) - fifo->size;

	if (data != NULL)
		memcpy(data, (char *)fifo->data + offset, size);

	return size;
}

void imq_clear_fifo(imq_fifo_t *fifo) {
	free(fifo->data);
	fifo->data = NULL;
	fifo->size = 0;
}

size_t imq_fifo_size(imq_fifo_t *fifo) {
	return fifo->size;
}

imq_fifo_t *imq_clone_fifo(imq_fifo_t *fifo) {
	imq_fifo_t *new_fifo;

	new_fifo = imq_alloc_fifo();

	if (new_fifo == NULL)
		return NULL;

	new_fifo->data = malloc(fifo->size);

	if (new_fifo->data == NULL && fifo->size > 0) {
		imq_free_fifo(new_fifo);
		return NULL;
	}

	memcpy(new_fifo->data, fifo->data, fifo->size);

	new_fifo->size = fifo->size;

	return new_fifo;
}
