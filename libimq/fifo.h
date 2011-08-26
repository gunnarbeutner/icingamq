#ifndef FIFO_H
#define FIFO_H

typedef enum imq_fifo_direction_e {
	FIFO_FROM_FD,
	FIFO_TO_FD
} imq_fifo_direction_t;

typedef struct imq_fifo_s {
	void *data;
	size_t size;
} imq_fifo_t;

imq_fifo_t *imq_alloc_fifo(void);
void imq_free_fifo(imq_fifo_t *fifo);
imq_fifo_t *imq_clone_fifo(imq_fifo_t *fifo);
int imq_splice_fifo(imq_fifo_t *fifo, int fd, imq_fifo_direction_t direction);
int imq_write_fifo(imq_fifo_t *fifo, const void *data, size_t size);
int imq_read_fifo(imq_fifo_t *fifo, void *data, size_t size);
int imq_peak_fifo(imq_fifo_t *fifo, void *data, size_t size, size_t offset);
void imq_clear_fifo(imq_fifo_t *fifo);
size_t imq_fifo_size(imq_fifo_t *fifo);

#endif /* FIFO_H */