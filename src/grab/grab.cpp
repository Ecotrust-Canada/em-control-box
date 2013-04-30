#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <errno.h>
#include <sys/stat.h>
#include <linux/videodev2.h>
#include "sys/time.h"

#include "../config.h"
#include "jpeglib.h"
#include "bmp.h"
#include "v4l2.h"
using namespace std;

static char LOG_BUFFER[255];
CONFIG_TYPE CONFIG;

char TMP_DIR[16] = "/tmp/";
char pathBuffer[64] = "";
int cam_working[] = {1, 1, 1, 1};
int cur_input = 0;
int awayFromHomePort = 0;
int num_cams;
int no_signal_counts[4] = {0, 0, 0, 0};
/* Default framerate. Should be overridden by config parameter in vms.conf */
int framerate = 2;
/* Width to resize cam images down to for display purposes */
int compress_width;
bool compress = false;

static char* timestamp(char* out) {
    static char DATE_BUF[80], TIME_BUF[80];
    time_t rawtime;
    struct tm * timeinfo;

    struct timeval  tv;
    struct timezone tz;
    struct tm *tm;

    // get date
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    strftime (DATE_BUF ,80, "%Y-%m-%d", timeinfo);

    // get time
    gettimeofday(&tv, &tz);
    tm = localtime(&tv.tv_sec);
    sprintf(TIME_BUF, "%02d-%02d.%02d.%06d",
        tm->tm_hour, tm->tm_min, tm->tm_sec, (unsigned int)tv.tv_usec);

    return out + sprintf(out, "%s_%s", DATE_BUF, TIME_BUF);
}

void write_jpeg(const char* filename, int image_width, int image_height, JSAMPLE *image_buffer) {
    int quality = 50;
    /* This struct contains the JPEG compression parameters and pointers to
     * working space (which is allocated as needed by the JPEG library).
     * It is possible to have several such structures, representing multiple
     * compression/decompression processes, in existence at once.  We refer
     * to any one struct (and its associated working data) as a "JPEG object".
     */
    struct jpeg_compress_struct cinfo;
    /* This struct represents a JPEG error handler.  It is declared separately
     * because applications often want to supply a specialized error handler
     * (see the second half of this file for an example).  But here we just
     * take the easy way out and use the standard error handler, which will
     * print a message on stderr and call exit() if compression fails.
     * Note that this struct must live as long as the main JPEG parameter
     * struct, to avoid dangling-pointer problems.
     */
    struct jpeg_error_mgr jerr;
    /* More stuff */
    FILE * outfile;              /* target file */
    JSAMPROW row_pointer[1];     /* pointer to JSAMPLE row[s] */
    int row_stride;              /* physical row width in image buffer */

    /**
     * Change the buffer color from BGR to RGB for jpeg.
     */
    int index;
    JSAMPLE tmp;
    for(int i = 0; i< image_height; i++)
        for(int j = 0; j< image_width; j++)
        {
            index = image_width * i * 3 + j * 3;
            tmp = image_buffer[index];
            image_buffer[index] = image_buffer[index + 2];
            image_buffer[index + 2] = tmp;
        }

    /* Step 1: allocate and initialize JPEG compression object */

    /* We have to set up the error handler first, in case the initialization
     * step fails.  (Unlikely, but it could happen if you are out of memory.)
     * This routine fills in the contents of struct jerr, and returns jerr's
     * address which we place into the link field in cinfo.
     */
    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    /* Step 2: specify data destination (eg, a file) */
    /* Note: steps 2 and 3 can be done in either order. */

    struct stat st, parent_st;
    char dirByDate[50] = "";
    char date[11] = "";
    int i = 0;
    for(i = 0; i < 10; i++)
        date[i] = filename[i];

    stat(CONFIG.DATA_MNT, &st);
    sprintf(pathBuffer, "%s%s", CONFIG.DATA_MNT, "/../");
    stat(pathBuffer, &parent_st);

    // DATA_MNT's (/mnt/data) device ID is the same as its parent (/mnt), meaning
    // there is no data drive mounted
    if (st.st_dev == parent_st.st_dev) {
        // use backup location
        sprintf(pathBuffer, "%s%s", CONFIG.BACKUP_DATA_MNT, "/");
        strcpy(dirByDate, pathBuffer);
    } else {
        sprintf(pathBuffer, "%s%s", CONFIG.DATA_MNT, "/");
        strcpy(dirByDate, pathBuffer);
    }

    strcat(dirByDate, date);
    // if dir doesn't exist try to make it
    if(stat(dirByDate, &st) != 0) {
        char dirCommand[50] = "mkdir ";
        strcat(dirCommand, dirByDate);
        if (system(dirCommand)) {
            cerr << "ERROR: Failed to create DATE directory.";
        };
        char tmp[] = "00";
        char tmpCommand[50] = "";
        for(i = 0; i < 24; i++) {
            strcpy(tmpCommand, dirCommand);
            strcat(tmpCommand, "/");
            tmp[0] = i/10 + '0';
            tmp[1] = i%10 + '0';
            strcat(tmpCommand, tmp);
            if (system(tmpCommand)) {
                cerr << "ERROR: Failed to create HOUR directory.";
            }
        }
    }

    /* Here we use the library-supplied code to send compressed data to a
     * stdio stream.  You can also write your own code to do something else.
     * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
     * requires it in order to write binary files.
     */
    strcat(dirByDate, "/");
    char fullFilename[100] = "";
    strcpy(fullFilename, dirByDate);
    char hour[3] = "";
    hour[0] = filename[11];
    hour[1] = filename[12];
    strcat(fullFilename, hour);
    strcat(fullFilename, "/");
    strcat(fullFilename, filename);
    if ((outfile = fopen(fullFilename, "wb")) == NULL)
    {
        fprintf(stderr, "can't open %s\n", fullFilename);
        exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);

    /* Step 3: set parameters for compression */

    /* First we supply a description of the input image.
     * Four fields of the cinfo struct must be filled in:
     */
    /* image width and height, in pixels */
    cinfo.image_width = image_width;
    cinfo.image_height = image_height;
    cinfo.input_components = 3;  /* # of color components per pixel */
                                 /* colorspace of input image */
    cinfo.in_color_space = JCS_RGB;
    /* Now use the library's routine to set default compression parameters.
     * (You must set at least cinfo.in_color_space before calling this,
     * since the defaults depend on the source color space.)
     */
    jpeg_set_defaults(&cinfo);
    /* Now you can set any non-default parameters you wish to.
     * Here we just illustrate the use of quality (quantization table) scaling:
     */
    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
    /* Step 4: Start compressor */

    /* TRUE ensures that we will write a complete interchange-JPEG file.
     * Pass TRUE unless you are very sure of what you're doing.
     */
    jpeg_start_compress(&cinfo, TRUE);

    /* Step 5: while (scan lines remain to be written) */
    /*           jpeg_write_scanlines(...); */

    /* Here we use the library's state variable cinfo.next_scanline as the
     * loop counter, so that we don't have to keep track ourselves.
     * To keep things simple, we pass one scanline per call; you can pass
     * more if you wish, though.
     */
    row_stride = image_width * 3;/* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height)
    {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could pass
         * more than one scanline at a time if that's more convenient.
         */
        row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    /* Step 6: Finish compression */

    jpeg_finish_compress(&cinfo);
    /* After finish_compress, we can close the output file. */
    fclose(outfile);

    /* Step 7: release JPEG compression object */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_compress(&cinfo);

    if(compress)
    {
        string imgname = "tmpi.bmp";
        string disimgname = "cami.bmp";
        imgname[3] = cur_input + '0';
        disimgname[3] = cur_input + '0';
        string dir = "/tmp/";

        save_bmp(compress_width, dir + imgname, (unsigned char*)image_buffer);
        string command = "mv " + dir + imgname + " " + dir + disimgname;
        if (system(command.c_str())) {
            cerr << "ERROR: Failed to move camera page buffer file";
        }
    }
    else
    {
        /* Create a symbolic link file called frame.jpg for the GUI.*/
        char command[150] = "ln -s -f ";
        char camframe[] = "cami.bmp";
        camframe[3] = cur_input + '0';
        strcat(command, fullFilename);
        strcat(command, " ");
        strcat(command, TMP_DIR);
        strcat(command, camframe);
        if (system(command)) {
            cerr << "ERROR: Failed to create camera link";
        }
    }
}

static void process_no_signal() {

    char tmp[150] = "";
    strcat(tmp, TMP_DIR);
    char camframe[20] = "cami.bmp";
    camframe[3] = cur_input + '0';
    strcat(tmp, camframe);
    char tmpcom[100] = "rm -f ";
    strcat(tmpcom, tmp);
    strcat(tmpcom, " && ln -s /tmp/no-signal.png ");
    strcat(tmpcom, tmp);
    if(system(tmpcom)) {
        cerr << "ERROR: Linking failed to no-signal failed";
    }
}

static void process_image(const void *p) {
    // get filename
    char filename[50];
    char* cursor;
    cursor = timestamp(filename);
    sprintf(cursor, ".I%d.jpg", cur_input);

    /**
     * Check whether the camera with index 'input' is working or not.
     * It is checked by whether the output image is pure red image, i.e., the buffer consists only of -64 or 0.
     * For efficiency only some portions of the buffer are checked.
     * TEST_COUNT denotes the number of subregion to be checked and the array 'index' contains the index
     * of the first byte of corresponding subregion.
     * If some carame is detected to be not working, we decrease num_cams, which denotes the number of cameras in use.
     */
    const int TEST_LENGTH = 2000;
    const int TEST_COUNT = 3;
    const int index[] = {1044, 7044, 31044};
    int flag = 0, j = 0, i = 0, start = 0, end = 0;

    for(i = 0; i < TEST_COUNT; i++)
    {
        start = index[i], end = start + TEST_LENGTH;
        for(; start< end; start+= 3)
        {
            for(j = 0; j < 3; j++)
            {
                if(((char *)p)[start+j]!= -64 && ((char *)p)[start+j] && ((char *)p)[start+j])
                    flag = 1;
            }
        }
    }
    
    if(flag)
    {
        // display the first good camera in the GUI
        if(!awayFromHomePort) sprintf(filename,"0000-00-00_00-00.00.00000%d.I%d.jpg", page, cur_input);
        write_jpeg(filename,640,480,(unsigned char*)p);
    }
    else
    {
        //printf("INFO: No data from camera #%d.\n", cur_input, num_cams);
        no_signal_counts[cur_input] = (no_signal_counts[cur_input]+1)% 10;
        process_no_signal();
    }
}

static int read_frame(void) {
    struct v4l2_buffer buf;
    unsigned int i;
    //extern struct buffer *         buffers;

    switch (io)
    {
        case IO_METHOD_READ:
            if (-1 == read (fd, buffers[0].start, buffers[0].length))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("read");
                }
            }

            process_image (buffers[0].start);

            break;

        case IO_METHOD_MMAP:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            assert (buf.index < n_buffers);

            process_image (buffers[buf.index].start);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;

        case IO_METHOD_USERPTR:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long) buffers[i].start
                && buf.length == buffers[i].length)
                    break;

            assert (i < n_buffers);

            process_image ((void *) buf.m.userptr);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;
    }

    return 1;
}

static void mainloop(void) {
    int i = 0, active_count = 0;
    num_cams = 4;
    char* log_cursor;
    double start = 0; 
    unsigned int time_to_wait = 0;

    printf("INFO:framerate=%d, num_cams=%d\n", framerate, num_cams);
    printf("INFO:Now recording\n");

    while (1) {
        start = now();

        log_cursor = timestamp(LOG_BUFFER);
        *log_cursor = '\0';

        if (cur_input==0) page = 1-page;
        switch_input(cur_input);
        struct stat st;
        sprintf(pathBuffer, "%s%s", CONFIG.DATA_MNT, "/.pause-grab");
        awayFromHomePort = stat(pathBuffer, &st);
        
        /* unless we have a (camera specific) number of no-signal events,
         * don't check the signal.
         */
        if (no_signal_counts[cur_input] != 3 * cur_input) {
            no_signal_counts[cur_input] = (no_signal_counts[cur_input]+1) % 10;
            process_no_signal();
            cur_input = (cur_input + 1) % num_cams;
            continue;
        }

        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO (&fds);
            FD_SET (fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select (fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r)
            {
                if (EINTR == errno)
                    continue;

                errno_exit ("select");
            }

            if (0 == r)
            {
                fprintf (stderr, "select timeout\n");
                exit (EXIT_FAILURE);
            }

            if (read_frame ())
                break;

            /* EAGAIN - continue select loop. */
        }

        active_count = 4;
        for(i = 0; i< 4; i++)
            if(no_signal_counts[i] != 3 * i)
                active_count --;

        time_to_wait = (unsigned int) (1000000.0/active_count/framerate - (now() - start));
        if (time_to_wait < 0 || time_to_wait > 1000000) time_to_wait = 0;
        cur_input = (cur_input + 1) % num_cams;
        usleep(time_to_wait);
    }
}

int main (int argc, char *argv[]) {
    char cmd[127];
    int screenWidth = 1024;

    if (argc < 3) {
        printf("Usage: vms-grab [device] [resolution]\n");
        exit(1);
    }

    sscanf(argv[1], "%s", dev_name);
    sscanf(argv[2], "%d", &screenWidth);

    if((screenWidth - 264)/2 < 640) {
        compress = true;
        compress_width = (screenWidth - 264)/2;
    }

    if (readConfigFile()) {
        cerr << "ERROR: Failed to get configuration" << endl;
        exit(-1);
    };

    strcpy(CONFIG.framerate, getConfig("framerate"));
    strcpy(CONFIG.DATA_MNT, getConfig("DATA_MNT"));
    strcpy(CONFIG.BACKUP_DATA_MNT, getConfig("BACKUP_DATA_MNT"));

    // use resized no signal image
    if(compress) {
        sprintf(cmd, "convert -geometry %d /usr/local/apps/vms_recording/static/no-signal-original.png %sno-signal.png", compress_width, TMP_DIR);
        if (system(cmd)) cerr << "ERROR: Failed to resize no-signal image";
    // use original size no signal image
    } else {
        sprintf(cmd, "cp /usr/local/apps/vms_recording/static/no-signal-original.png %sno-signal.png", TMP_DIR);
        if (system(cmd)) cerr << "ERROR: Failed to copy no-signal image";
    }

    open_device();

    init_device();

    start_capturing();

    mainloop();

    stop_capturing();

    uninit_device();

    close_device();

    return 0;
}