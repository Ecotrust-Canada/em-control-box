#ifndef V4L2_H
#define V4L2_H

typedef enum {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

void errno_exit(const char*);

int xioctl(int, int, void*);

void start_capturing(void);
void stop_capturing(void);
void open_device(void);
void close_device(void);
void init_device(void);
void uninit_device(void);
void switch_input(int);

struct buffer {
    void * start;
    size_t length;
};

extern struct buffer * buffers;
extern char dev_name[32];
extern int fd;
extern int page;
extern io_method io;
extern unsigned int n_buffers;

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#endif