/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 * Display screen test
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include "whiteboard.h"
//For test to be remove
#include <png.h>
#include <zlib.h>

int main() {
    int fd = WhiteBoard::getWhiteBoardFd();
    void* mapBase = nullptr;
    // 33177600 = 3840*2160*4
    mapBase = mmap(NULL, 33177600, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapBase == MAP_FAILED) {
        fprintf(stderr, "mmap failed\n");
    }

    //open the picture and read the picture
    //For test display remove soon
    const char * pngPath  = "/system/media/UI.png";
    FILE *file = fopen(pngPath,"rb");
    if (file == NULL) {
        fprintf(stderr,"Unable to open PNG %s \n",pngPath);
        return 0;
    }

    //use libpng
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));
    png_init_io(png_ptr, file);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
    int m_width = png_get_image_width(png_ptr, info_ptr);
    int m_height = png_get_image_height(png_ptr, info_ptr);

    unsigned char ** row_pointers = png_get_rows(png_ptr, info_ptr);
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if (row_pointers)
        fprintf(stderr,"Raw Data size(%dx%d) row_bytes %d \n", m_width, m_height,row_bytes);

    for (int i = 0; i < m_height; i++) {
        memcpy((char *)mapBase + row_bytes*i, row_pointers[i], row_bytes);
    }

    fprintf(stderr, "White the data to White board finish!\n");
    return 0;
}
