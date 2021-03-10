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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>

static void calc_ssim_window(uint8_t *org, uint8_t *enc, int stride, int winW, int winH,
                             int *variance_org, int *variance_enc, int *covariance,
                             int *mean_org, int *mean_enc)
{
    *variance_org=0;
    *variance_enc=0;
    *covariance=0;
    *mean_org=0;
    *mean_enc=0;
    int i,j;

    for (i=0; i<winH; i++) {
        for (j=0; j<winW; j++) {
            *mean_org += org[i*stride + j];
            *mean_enc += enc[i*stride + j];
            *variance_org += org[i*stride + j] * org[i*stride + j];
            *variance_enc += enc[i*stride + j] * enc[i*stride + j];
            *covariance	+= org[i*stride + j] * enc[i*stride + j];
        }
    }
}

static double calc_ssim_frame(uint8_t *org, uint8_t *enc, int width, int height)
{
    //SSIM parameters, default
    int L = (2<<8 ) - 1; // for 8-bit YUV
    int winH=8;
    int winW=8;
    int winStepW = 1;
    int winStepH = 1;
    double C1 = (double)(L*L)*0.01*0.01;
    double C2 = (double)(L*L)*0.03*0.03;
    double winSize = (double)(winH * winW);

    double cur_distortion = 0.0f;
    int win_count =0,i,j;
    for (i = 0; i <= width - winW; i += winStepW) {
        for (j = 0; j <= height - winH; j += winStepH) {
            int variance_org=0, variance_enc=0, covariance=0;
            int mean_org=0, mean_enc=0;
            // todo: speed up...
            calc_ssim_window(&org[j*width + i], &enc[j*width + i],
                             width, winW, winH,
                             &variance_org, &variance_enc, &covariance, &mean_org, &mean_enc);

            double d_mean_org = (double)mean_org / winSize;
            double d_mean_enc = (double)mean_enc / winSize;

            double ssim_n  = (double)((2.0 * d_mean_org * d_mean_enc + C1)
                                      * (2.0 * (covariance*1.0/winSize - d_mean_org*d_mean_enc ) + C2));
            double ssim_d = (double)( d_mean_org* d_mean_org + d_mean_enc* d_mean_enc +C1) *
                (double)(variance_org*1.0/winSize - d_mean_org* d_mean_org +
                         variance_enc*1.0/winSize - d_mean_enc* d_mean_enc + C2);

            cur_distortion += ssim_n/ssim_d;
            win_count ++;
        }
    }

    cur_distortion /= (double)win_count;

    return cur_distortion;
}

int calc_SSIM(FILE *srcyuv_fp, FILE *destyuv_fp, int frame_width, int frame_height, int frame_count,
              double *ssim)
{
    int frm_num = 0;
    uint8_t *buf[2];
    int frame_size = (int)(frame_width * frame_height * 1.5);

    buf[0] = malloc(frame_size);
    buf[1] = malloc(frame_size);

    fseek(srcyuv_fp, 0, SEEK_SET);
    fseek(destyuv_fp, 0, SEEK_SET);

    int s0, s1;
    while (frm_num < frame_count) {
        s0 = fread(buf[0], 1, frame_size, srcyuv_fp);
        s1 = fread(buf[1], 1, frame_size, destyuv_fp);
        if ((s0 == 0) || (s1 == 0) || (s0 != s1)){
            fprintf(stderr, "reading from src or rec yuv files error\n");
	    break;
        }
        frm_num++;
        *ssim += calc_ssim_frame(buf[0], buf[1], frame_width, frame_height);
    }

    *ssim /= frm_num;

    //printf("IMAGE QUALITY: yssim = %.4f\n\n", *ssim);
    return 0;
}
