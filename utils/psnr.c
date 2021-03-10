/*
 * Copyright (c) 2007-2013 Intel Corporation. All Rights Reserved.
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
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Authors:
   Yuan, Shengquan<shengquan.yuan@intel.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <math.h>
#include <fcntl.h>

int calc_PSNR(FILE *srcyuv_fp, FILE *destyuv_fp, int frame_width, int frame_height, int frame_count,
              double *psnr, double *psnr_y, double *psnr_u, double *psnr_v, double *mse)
{
    char *srcyuv_ptr = NULL, *recyuv_ptr = NULL, tmp;
    unsigned int min_size, i;
    double double_tmp=0, sse=0, sse_y=0, sse_u=0, sse_v=0;
    int fourM = 0x400000; /* 4M */
    int yuv_size = frame_width * frame_height * 1.5;
    int y_size = frame_width * frame_height;
    
    min_size = frame_count * yuv_size;
    for (i=0; i<min_size; i++) {
        unsigned int j = i % fourM;
        unsigned int frame_offset = i % yuv_size;

        if (j == 0) {
            if (srcyuv_ptr)
                munmap(srcyuv_ptr, fourM);
            if (recyuv_ptr)
                munmap(recyuv_ptr, fourM);
            
            srcyuv_ptr = mmap64(0, fourM, PROT_READ, MAP_SHARED, fileno(srcyuv_fp), (off64_t)i);
            recyuv_ptr = mmap64(0, fourM, PROT_READ, MAP_SHARED, fileno(destyuv_fp), (off64_t)i);
            if ((srcyuv_ptr == MAP_FAILED) || (recyuv_ptr == MAP_FAILED)) {
                printf("Failed to mmap YUV files\n");
                return 1;
            }
        }
        tmp = srcyuv_ptr[j] - recyuv_ptr[j];
        double_tmp = tmp * tmp;

        sse += double_tmp;
        if (frame_offset < y_size) /* Y  */
            sse_y += double_tmp;
        else {
            int uv_offset = frame_offset - y_size;
            if (uv_offset&1)
                sse_v += double_tmp;
            else
                sse_u += double_tmp;
        }
    }
    *mse = (double)sse/(double)min_size;
    *psnr = 20.0*log10(255) - 10.0*log10(*mse);
    *psnr_y = 20.0*log10(255) - 10.0*log10((double)sse_y/(double)(min_size*2/3));
    *psnr_u = 20.0*log10(255) - 10.0*log10((double)sse_u/(double)(min_size*1/6));
    *psnr_v = 20.0*log10(255) - 10.0*log10((double)sse_v/(double)(min_size*1/6));

    if (srcyuv_ptr)
        munmap(srcyuv_ptr, fourM);
    if (recyuv_ptr)
        munmap(recyuv_ptr, fourM);
    
    return 0;
}
