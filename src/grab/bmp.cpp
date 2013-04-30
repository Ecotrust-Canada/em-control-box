// Utility for saving a bitmap to a file using a callback function.
//
#include <cmath>
#include "bmp.h"
using namespace std;

const int EPS = 10E-5;

unsigned char newbuf[480][640][3];
double whmult = 0;
int iwhmult[640], sum[640][3];
bool flag = false;

int save_bmp(int newWidth, string filename, unsigned char *buf) {
	char *ptr;
	char id[2];
	BMPHEAD bh;
    int oldw = 640;
    int oldh = 480;
    double ratio = 640.0/newWidth;
    int newHeight = floor(oldh / ratio);
    int firsth, lasth, firstw, lastw;
    int io3;
    int index[3] = {2, 1, 0};

    if(!flag)
    {
        flag = true;
        iwhmult[0] = 0;
        for(int i = 1; i< newWidth; i++)
        {
            whmult += ratio;
            iwhmult[i] = (int)whmult;
        }
    }

    //Resize the image.
    for(int h = 0; h< newHeight; h++)
    {
        firsth = iwhmult[h], lasth = iwhmult[h+1];
        memset(sum, 0, sizeof(sum));
        for(int i = firsth; i <= lasth; i++)
        {
            io3 = i*oldw*3;
            for(int w = 0; w< newWidth; w++)
            {
                firstw = iwhmult[w], lastw = iwhmult[w+1];
                for(int j = firstw; j<= lastw; j++)
                {
                    for(int k = 0; k< 3; k++)
                        sum[w][k] += buf[io3 + j*3 + index[k]];
                }
            }
        }
        for(int w = 0; w< newWidth; w++) 
            for(int kk = 0; kk< 3; kk++)
                newbuf[h][w][kk] = sum[w][kk]/ ((iwhmult[w+1]-iwhmult[w]+1)*(lasth-firsth+1));
    }

	memset ((char *)&bh,0,sizeof(BMPHEAD)); /* sets entire header to 0 */
	id[0] = 'B';
	id[1] = 'M';
	/* bh.filesize  = calculated size of your file (see below)*/
	/* bh.reserved  = two zero bytes */
	bh.headersize  = 54;  /* (for 24 bit images) */
	bh.infoSize  =  0x28;  /*(for 24 bit images) */
	bh.width     = newWidth; /* width in pixels of your image */
	bh.height     = newHeight; /* depth in pixels of your image */
	bh.biPlanes  =  1; /* (for 24 bit images) */
	bh.bits      = 24; /* (for 24 bit images) */
	bh.biCompression = 0; /* (no compression) */
	int bytesPerLine;

	bytesPerLine = bh.width * 3;  /* (for 24 bit images) */
	/* round up to a dword boundary */
	if (bytesPerLine & 0x0003) 
	{
	    bytesPerLine |= 0x0003;
	    ++bytesPerLine;
	}

	bh.filesize = bh.headersize+(long)bytesPerLine*bh.height;
	FILE * bmpfile;

	bmpfile = fopen(filename.c_str(), "wb");

	if (bmpfile == NULL)
	{
		printf("Error opening output file\n");
		return 1;
	}

	fwrite(id, 2, sizeof (char), bmpfile);
	fwrite(&bh, 1, sizeof (bh), bmpfile);

	char linebuf[10000];

	//linebuf = (char *) calloc(1, bytesPerLine);

	if (linebuf == NULL)
	{
		printf ("Error allocating memory\n");
		fclose(bmpfile);
		return(1);
	}

	for (int line = bh.height - 1; line >= 0; line --)  {
		ptr = linebuf;
		for (int col = 0; col < bh.width; col++) {
			
			// write colors for pixel to bitmap memory.
			*ptr = (unsigned int)(newbuf[line][col][0]);
			*(ptr+1) = (unsigned int)(newbuf[line][col][1]);
			*(ptr+2) = (unsigned int)(newbuf[line][col][2]);
			ptr += 3;
		}
		fwrite(linebuf, 1, bytesPerLine, bmpfile);
	}

	fclose(bmpfile);
	return 0;
}