#ifndef BMP_H
#define BMP_H

#include <cstdio>
#include <cstring>
#include <string>
using namespace std;

typedef struct {
	int filesize;
	char reserved[2];
	int headersize;
	int infoSize;
	int width;
	int height;
	short biPlanes;
	short bits;
	int biCompression;
	int biSizeImage;
	int biXPelsPerMeter;
	int biYPelsPerMeter;
	int biClrUsed;
	int biClrImportant;
} BMPHEAD;


/* saves a bitmap file */
int save_bmp(int, string, unsigned char*);

#endif