/*
 * Copyright (c) 2008-2009 Intel Corporation. All Rights Reserved.
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
#include "loadsurface_yuv.h"
//#include <stdio.h>

static int upload_surface(VADisplay va_dpy, VASurfaceID surface_id,
                          int box_width, int row_shift,
                          int field)
{
    VAImage surface_image;
    void *surface_p=NULL, *U_start = NULL,*V_start = NULL;
    VAStatus va_status;
    unsigned int pitches[3]={0,0,0};

    va_status = vaDeriveImage(va_dpy,surface_id,&surface_image);
    CHECK_VASTATUS(va_status,"vaDeriveImage");

    vaMapBuffer(va_dpy,surface_image.buf,&surface_p);
    CHECK_VASTATUS(va_status,"vaMapBuffer");
    
    pitches[0] = surface_image.pitches[0];
    switch (surface_image.format.fourcc) {
    case VA_FOURCC_NV12:
        U_start = (char *)surface_p + surface_image.offsets[1];
        V_start = (char *)U_start + 1;
        pitches[1] = surface_image.pitches[1];
        pitches[2] = surface_image.pitches[1];
        break;
    case VA_FOURCC_IYUV:
        U_start = (char *)surface_p + surface_image.offsets[1];
        V_start = (char *)surface_p + surface_image.offsets[2];
        pitches[1] = surface_image.pitches[1];
        pitches[2] = surface_image.pitches[2];
        break;
    case VA_FOURCC_YV12:
        U_start = (char *)surface_p + surface_image.offsets[2];
        V_start = (char *)surface_p + surface_image.offsets[1];
        pitches[1] = surface_image.pitches[2];
        pitches[2] = surface_image.pitches[1];
        break;
    case VA_FOURCC_YUY2:
        U_start = (char *)surface_p + 1;
        V_start = (char *)surface_p + 3;
        pitches[1] = surface_image.pitches[0];
        pitches[2] = surface_image.pitches[0];
        break;
    default:
        assert(0);
    }
    /* assume surface is planar format */
    yuvgen_planar(surface_image.width, surface_image.height,
                  (unsigned char *)surface_p, pitches[0],
                  (unsigned char *)U_start, pitches[1],
                  (unsigned char *)V_start, pitches[2],
                  surface_image.format.fourcc,
                  box_width, row_shift, field);
        
    vaUnmapBuffer(va_dpy,surface_image.buf);

    vaDestroyImage(va_dpy,surface_image.image_id);
    
    return 0;
}

/*
 * Upload YUV data from memory into a surface
 * if src_fourcc == NV12, assume the buffer pointed by src_U
 * is UV interleaved (src_V is ignored)
 */
static int upload_surface_yuv(VADisplay va_dpy, VASurfaceID surface_id,
                              int src_fourcc, int src_width, int src_height,
                              unsigned char *src_Y, unsigned char *src_U, unsigned char *src_V)
{
    VAImage surface_image;
    unsigned char *surface_p=NULL, *Y_start=NULL, *U_start=NULL, *V_start=NULL;
    int Y_pitch=0, U_pitch=0, V_pitch=0, row;
    VAStatus va_status;
    int buf_width, buf_height;
    int pad_width, pad_height;
    unsigned char *U_row = NULL;
    unsigned char *u_ptr = NULL, *v_ptr=NULL;
    int j;

    
    va_status = vaDeriveImage(va_dpy,surface_id, &surface_image);
    CHECK_VASTATUS(va_status,"vaDeriveImage");

    vaMapBuffer(va_dpy,surface_image.buf,(void **)&surface_p);
    assert(VA_STATUS_SUCCESS == va_status);

    Y_start = surface_p;
    Y_pitch = surface_image.pitches[0];
    switch (surface_image.format.fourcc) {
    case VA_FOURCC_NV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        V_start = U_start + 1;
        U_pitch = surface_image.pitches[1];
        V_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_IYUV:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        V_start = (unsigned char *)surface_p + surface_image.offsets[2];
        U_pitch = surface_image.pitches[1];
        V_pitch = surface_image.pitches[2];
        break;
    case VA_FOURCC_YV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[2];
        V_start = (unsigned char *)surface_p + surface_image.offsets[1];
        U_pitch = surface_image.pitches[2];
        V_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_YUY2:
        U_start = surface_p + 1;
        V_start = surface_p + 3;
        U_pitch = surface_image.pitches[0];
        V_pitch = surface_image.pitches[0];
        break;
    default:
        assert(0);
    }

    buf_width = surface_image.pitches[0];
    buf_height = surface_image.offsets[1] / surface_image.pitches[0];
    pad_width = buf_width - src_width;
    pad_height = buf_height - src_height;

    /* copy Y plane */
    for (row = 0;row < src_height; row++) {
        unsigned char *Y_row = Y_start + row * Y_pitch;
        memcpy(Y_row, src_Y + row*src_width, src_width);
        memset(Y_row + src_width, 0, pad_width);
    }
    for (;row < buf_height; row++) {
        unsigned char *Y_row = Y_start + row * Y_pitch;
        memset(Y_row, 0, buf_width);
    }

    if (surface_image.format.fourcc == VA_FOURCC_NV12) {
        U_row = U_start;
        if (src_fourcc == VA_FOURCC_NV12) {
            for (row = 0; row < (src_height >> 1); row++) {
               memcpy(U_row, src_U + row * src_width, src_width);
               memset(U_row + src_width, 0, pad_width);
               U_row += U_pitch;
            }
            for (; row < (buf_height >> 1); row++) {
                memset(U_row, 0, buf_width);
                U_row += U_pitch;
            }
        } else {
            u_ptr = src_U; v_ptr = src_V;

            for (row = 0; row < (src_height >> 1); row++) {
                for(j = 0; j < (src_width >> 1); j++) {
                    U_row[2*j] = *u_ptr++;
                    U_row[2*j+1] = *v_ptr++;
                }
                U_row += U_pitch;
            }
        }
    } else {
        //VA_FOURCC_IYUV:
        //VA_FOURCC_YV12:
        //VA_FOURCC_YUY2:
        printf("the fourcc is %d\n", surface_image.format.fourcc);
        printf("unsupported fourcc in load_surface_yuv\n");
        assert(0);
    }

    vaUnmapBuffer(va_dpy,surface_image.buf);

    vaDestroyImage(va_dpy,surface_image.image_id);

    return 0;
}

/*
 * Download YUV data from a surface into memory
 * Some hardward doesn't have a aperture for linear access of
 * tiled surface, thus use vaGetImage to expect the implemnetion
 * to do tile to linear convert
 * 
 * if dst_fourcc == NV12, assume the buffer pointed by dst_U
 * is UV interleaved (src_V is ignored)
 */
static int download_surface_yuv(VADisplay va_dpy, VASurfaceID surface_id,
                                int dst_fourcc, int dst_width, int dst_height,
                                unsigned char *dst_Y, unsigned char *dst_U, unsigned char *dst_V)
{
    VAImage surface_image;
    VAImageFormat format;
    unsigned char *surface_p=NULL, *Y_start=NULL, *U_start=NULL,*V_start=NULL;
    int Y_pitch=0, U_pitch=0, V_pitch=0, row;
    VAStatus va_status;
    
    va_status = vaDeriveImage(va_dpy,surface_id, &surface_image);
    if (VA_STATUS_SUCCESS != va_status) {
	format.fourcc = VA_FOURCC_NV12;
	va_status = vaCreateImage(va_dpy, &format, dst_width, dst_height, &surface_image);
	CHECK_VASTATUS(va_status,"vaCreateImage");
	va_status = vaGetImage(va_dpy, surface_id, 0, 0, dst_width, dst_height, surface_image.image_id);
	CHECK_VASTATUS(va_status,"vaGetImage");
    }

    vaMapBuffer(va_dpy,surface_image.buf,(void **)&surface_p);
    assert(VA_STATUS_SUCCESS == va_status);

    Y_start = surface_p;
    Y_pitch = surface_image.pitches[0];
    switch (surface_image.format.fourcc) {
    case VA_FOURCC_NV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        V_start = U_start + 1;
        U_pitch = surface_image.pitches[1];
        V_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_IYUV:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        V_start = (unsigned char *)surface_p + surface_image.offsets[2];
        U_pitch = surface_image.pitches[1];
        V_pitch = surface_image.pitches[2];
        break;
    case VA_FOURCC_YV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[2];
        V_start = (unsigned char *)surface_p + surface_image.offsets[1];
        U_pitch = surface_image.pitches[2];
        V_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_YUY2:
        U_start = surface_p + 1;
        V_start = surface_p + 3;
        U_pitch = surface_image.pitches[0];
        V_pitch = surface_image.pitches[0];
        break;
    default:
        assert(0);
    }

    /* copy Y plane */
    for (row=0;row<dst_height;row++) {
        unsigned char *Y_row = Y_start + row * Y_pitch;
        memcpy(dst_Y + row*dst_width, Y_row, dst_width);
    }
  
    for (row =0; row < dst_height/2; row++) {
        unsigned char *U_row = U_start + row * U_pitch;
        unsigned char *u_ptr = NULL, *v_ptr = NULL;
        int j;
        switch (surface_image.format.fourcc) {
        case VA_FOURCC_NV12:
            if (dst_fourcc == VA_FOURCC_NV12) {
                memcpy(dst_U + row * dst_width, U_row, dst_width);
                break;
            } else if (dst_fourcc == VA_FOURCC_IYUV) {
                u_ptr = dst_U + row * (dst_width/2);
                v_ptr = dst_V + row * (dst_width/2);
            } else if (dst_fourcc == VA_FOURCC_YV12) {
                v_ptr = dst_U + row * (dst_width/2);
                u_ptr = dst_V + row * (dst_width/2);
            }
            for(j = 0; j < dst_width/2; j++) {
                u_ptr[j] = U_row[2*j];
                v_ptr[j] = U_row[2*j+1];
            }
            break;
        case VA_FOURCC_IYUV:
        case VA_FOURCC_YV12:
        case VA_FOURCC_YUY2:
        default:
            printf("unsupported fourcc in load_surface_yuv\n");
            assert(0);
        }
    }
    
    vaUnmapBuffer(va_dpy,surface_image.buf);

    vaDestroyImage(va_dpy,surface_image.image_id);

    return 0;
}
