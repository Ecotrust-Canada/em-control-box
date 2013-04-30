#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "v4l2.h"

char dev_name[32];
struct buffer *         buffers         = NULL;
int              fd              = -1;
int              page            = 0;
io_method        io              = IO_METHOD_READ;
//static io_method        io              = IO_METHOD_MMAP;
unsigned int     n_buffers       = 0;

void
errno_exit                      (const char *           s)
{
    fprintf (stderr, "%s error %d, %s\n",
        s, errno, strerror (errno));

    exit (EXIT_FAILURE);
}


int
xioctl                          (int                    fd,
int                    request,
void *                 arg)
{
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);

    return r;
}

void switch_input(int index) {
    if (-1 == ioctl (fd, VIDIOC_S_INPUT, &index)) {
        perror ("VIDIOC_S_INPUT");
        exit (EXIT_FAILURE);
    }
}

struct v4l2_input cam_inputs[4];

void check_cams()
{
    for (int i = 0; i<4; i++)
    {
        cam_inputs[i].index = i;
        switch_input(i);
        usleep(1000000);
        memset (&cam_inputs[i], 0, sizeof (cam_inputs[i]));

        if (-1 == ioctl (fd, VIDIOC_ENUMINPUT, &cam_inputs[i]))
        {
            perror ("VIDIOC_ENUM_INPUT");
            exit (EXIT_FAILURE);
        }
        printf("INFO:CAM %d STATUS: %X\n", i, cam_inputs[i].status);

        if (cam_inputs[i].status & V4L2_IN_ST_NO_SIGNAL)
        {
            fprintf(stderr, "NO SIGNAL CAMERA");

            continue;

        }

        if (cam_inputs[i].status & V4L2_IN_ST_NO_POWER)
        {
            fprintf(stderr, "NO POWER CAMERA");
            continue;
        }

        struct v4l2_format format;
        memset(&format, 0, sizeof(format));
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        int result = ioctl(fd, VIDIOC_G_FMT, &format);
        if(result == -1)
        {
            printf("Failed\n");
        }
        format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
        result = ioctl(fd, VIDIOC_S_FMT, &format);
        if(result == -1)
        {
            switch (errno)
            {
                case EBUSY:
                    printf("Devices are busy\n");
                    break;
                case EINVAL:
                    printf("Invalid fields\n");
                    break;
                default:
                    printf("Unknown Error\n");
                    break;
            };
        } else {
            printf("Set succeeded\n");
        }

        result = ioctl(fd, VIDIOC_G_FMT, &format);
        printf("Cam #%d:\n", i);
        printf("%d %d %d\n", format.fmt.pix.height, format.fmt.pix.pixelformat, format.fmt.pix.colorspace);
    }
}


static void
init_mmap                       (void)
{
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s does not support "
                "memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2)
    {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
            dev_name);
        exit (EXIT_FAILURE);
    }

    buffers = (buffer *)calloc (req.count, sizeof (*buffers));

    if (!buffers)
    {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
            buf.length,
            PROT_READ | PROT_WRITE /* required */,
            MAP_SHARED /* recommended */,
            fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");
    }
}


static void
init_read                       (unsigned int           buffer_size)
{
    buffers = (buffer *)calloc (1, sizeof (*buffers));

    if (!buffers)
    {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);

    if (!buffers[0].start)
    {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
}


static void set_ntsc()
{
    struct v4l2_input input;
    v4l2_std_id std_id;

    memset (&input, 0, sizeof (input));

    if (-1 == ioctl (fd, VIDIOC_G_INPUT, &input.index))
    {
        perror ("VIDIOC_G_INPUT");
        exit (EXIT_FAILURE);
    }

    /* Note this is also supposed to work when only B
       or G/PAL is supported. */

    std_id = V4L2_STD_NTSC_M | V4L2_STD_NTSC_M_JP | V4L2_STD_NTSC_M_KR;

    if(-1 == ioctl (fd, VIDIOC_S_STD, &std_id))
    {
        perror ("VIDIOC_S_STD");
        exit (EXIT_FAILURE);
    }

}

void
stop_capturing                  (void)
{
    enum v4l2_buf_type type;

    switch (io)
    {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
                errno_exit ("VIDIOC_STREAMOFF");

            break;
    }
}

void
close_device                    (void)
{
    if (-1 == close (fd))
        errno_exit ("close");

    fd = -1;
}

void
open_device                     (void)
{
    struct stat st;

    if (-1 == stat (dev_name, &st))
    {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",
            dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!S_ISCHR (st.st_mode))
    {
        fprintf (stderr, "%s is no device\n", dev_name);
        exit (EXIT_FAILURE);
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd)
    {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
            dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}

void
start_capturing                 (void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    switch (io)
    {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
            {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = i;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
            {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_USERPTR;
                buf.index       = i;
                buf.m.userptr   = (unsigned long) buffers[i].start;
                buf.length      = buffers[i].length;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;
    }
}


void
init_device                     (void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s is no V4L2 device\n",
                dev_name);
            exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf (stderr, "%s is no video capture device\n",
            dev_name);
        exit (EXIT_FAILURE);
    }

    switch (io)
    {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE))
            {
                fprintf (stderr, "%s does not support read i/o\n",
                    dev_name);
                exit (EXIT_FAILURE);
            }

            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING))
            {
                fprintf (stderr, "%s does not support streaming i/o\n",
                    dev_name);
                exit (EXIT_FAILURE);
            }

            break;
    }

    /* Select video input, video standard and tune here. */

    CLEAR (cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap))
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;/* reset to default */

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop))
        {
            switch (errno)
            {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    }
    else
    {
        /* Errors ignored. */
    }

    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = 640;
    fmt.fmt.pix.height      = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    fmt.fmt.pix.colorspace  = V4L2_COLORSPACE_SRGB;

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
        errno_exit ("VIDIOC_S_FMT");

    check_cams();

    set_ntsc();
    /* Note VIDIOC_S_FMT may change width and height. */

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (io) {
        case IO_METHOD_READ:
            init_read(fmt.fmt.pix.sizeimage);
            break;

        case IO_METHOD_MMAP:
            init_mmap();
            break;

        case IO_METHOD_USERPTR:
            fprintf(stderr, "IO_METHOD_USERPTR is not supported\n");
            exit (EXIT_FAILURE);
            break;
    }
}

void
uninit_device                   (void)
{
    unsigned int i;

    switch (io)
    {
        case IO_METHOD_READ:
            free (buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap (buffers[i].start, buffers[i].length))
                    errno_exit ("munmap");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                free (buffers[i].start);
            break;
    }

    free (buffers);
}
