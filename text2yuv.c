/*
 * Copyright (c) 2009-2015 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL INTEL AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "asciiluma.h"


#define MAX_TEXT_NUM  1024
#define min(a,b) (a>b?b:a)

int char2luma(char c, int *width, int *height, int *stride, char **p)
{
    int row = (c-0x20) / 16;
    int col = (c-0x20) % 16;
    int bitmap_offset = row * (16*100*100) + col * 100;
    
    *p = &ascii_0x20_0x7f_luma_100x100[0] + bitmap_offset;
    *width = 100;
    *height = 100;
    *stride = 1600;
    
    printf("Character %c (item = %d) in (%d,%d), offset=%d\n",
           c, c - 0x20, row, col, bitmap_offset);

    return 0;
}

static int scale_2dimage(unsigned char *src_img, int src_imgw, int src_imgh,
                  unsigned char *dst_img, int dst_imgw, int dst_imgh)
{
    int row=0, col=0;

    for (row=0; row<dst_imgh; row++) {
        for (col=0; col<dst_imgw; col++) {
            *(dst_img + row * dst_imgw + col) = *(src_img + (row * src_imgh/dst_imgh) * src_imgw + col * src_imgw/dst_imgw);
        }
    }

    return 0;
}

static int help(char *exe)
{
    printf("Usage: %s -s <text> -w width -h height -f <YV12 file name>\n", exe);
    printf("Convert text to YV12 file with widthxheight resolution\n");
    return 0;
}

int main(int argc, char *argv[])
{
    int i,j, c, width=0, height=0, stride, yuv_width=0, yuv_height=0, text_num, uv_size;
    char *yuv_file = NULL, *char_luma[MAX_TEXT_NUM], uv_value;
    char *text_str = NULL;
    FILE *fp;
    
    while ((c =getopt(argc,argv,"w:h:s:f:?")) != EOF) {
        switch (c) {
        case 'w':
            yuv_width = atoi(optarg);
            break;
        case 'h':
            yuv_height = atoi(optarg);
            break;
        case 's':
            text_str = strdup(optarg);
            break;
        case 'f':
            yuv_file = strdup(optarg);
            break;
        default:
            help(argv[0]);
            exit(0);
        }
    }
    if (text_str == NULL || yuv_file == NULL) {
        help(argv[0]);
        exit(0);
    }
        
    fp = fopen(yuv_file, "w+");
    if (fp == NULL) {
        perror("Open file failed\n");
        exit(-1);
    }
    text_num = strlen(text_str);
    text_num = min(text_num, MAX_TEXT_NUM);
    
    for (i=0; i<text_num; i++) {
        char2luma(text_str[i], &width, &height, &stride, &char_luma[i]);
    }

    if (yuv_width == 0) yuv_width = width;
    if (yuv_height == 0) yuv_height = height;
    
    printf("Convert text %s to YV12 (%dx%d) and save to %s\n", text_str, yuv_width*text_num, yuv_height, yuv_file);
    /*
    printf("Get luma bitmap %dx%d, stride=%d, offset=%d\n", width, height, stride,
           char_luma - ascii_0x20_0x7f_luma_100x100);
    */
    for (i=0; i<height; i++) {
        for (j=0; j<text_num; j++) {
            int ret = fwrite(char_luma[j], width, 1, fp);
            if (ret < 1) {
                perror("Write less bytes than expected %d\n");
                exit(-1);
            }
            char_luma[j] += stride;            
        }
    }

    /* append UV data */
    uv_size = text_num * (width * height/2);
    uv_value = 0x80;
    for (i=0; i<uv_size; i++)
        fwrite(&uv_value, 1, 1, fp);

    printf("Converted to YV12!\n\n");
    
    return 0;
}
