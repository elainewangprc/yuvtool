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

/* Author: Wang, Elaine<elaine.wang@intel.com> */
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                  \
    ((unsigned long)(unsigned char) (ch0) | ((unsigned long)(unsigned char) (ch1) << 8) | \
     ((unsigned long)(unsigned char) (ch2) << 16) | ((unsigned long)(unsigned char) (ch3) << 24 ))

#define FOURCC_NV12 MAKEFOURCC('N','V','1','2')
#define FOURCC_YV12 MAKEFOURCC('Y','V','1','2')
#define FOURCC_I420 MAKEFOURCC('I','4','2','0')
#define FOURCC_IYUV MAKEFOURCC('I','Y','U','V')
#define FOURCC_BMP24 MAKEFOURCC('B','M','P','2')
#define FOURCC_YV16 MAKEFOURCC('Y','V','1','6')
#define FOURCC_YUV422P MAKEFOURCC('4','2','2','P')


#define CHECK_RET(ret) \
    while (0) {\
	if (ret != 0) \
	{\
	    fprintf(stderr, "%s L%d error %d\n", __FUNCTION__, __LINE__, ret);\
	    return ret;\
	}\
    }

typedef struct _BITMAPFILEHEADER
{
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BITMAPFILEHEADER;

typedef struct _BITMAPINFOHEADER
{
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_img;
    uint32_t x_ppm;
    uint32_t y_ppm;
    uint32_t clr_used;
    uint32_t clr_imp;
} BITMAPINFOHEADER;

/* Read bmp header from file*/
int bmp2yuv_read_bmp_file(FILE *bmpfile, 
	unsigned int *p_width, /*out*/
	unsigned int *p_height, /*out*/
	unsigned int *p_offset /*out*/)
{
    int ret = 0;
    BITMAPFILEHEADER bmp_header = {0};
    BITMAPINFOHEADER info_header = {0};
    if ( NULL == bmpfile ||
	    NULL == p_width ||
	    NULL == p_height ||
	    NULL == p_offset)
    {
	fprintf(stderr, "Invalid parameter!\n");
	return -1;
    }
    ret = fseek(bmpfile, 0, SEEK_SET);
    CHECK_RET(ret);
    fread( &bmp_header.type, sizeof(bmp_header.type), 1, bmpfile);
    fread( &bmp_header.size, sizeof(bmp_header.size), 1, bmpfile);
    fread( &bmp_header.reserved1, sizeof(bmp_header.reserved1), 1, bmpfile);
    fread( &bmp_header.reserved2, sizeof(bmp_header.reserved2), 1, bmpfile);
    fread( &bmp_header.offset, sizeof(bmp_header.offset), 1, bmpfile);

    //printf("Type 0x%x, size %d, offset %d\n", bmp_header.type, bmp_header.size, bmp_header.offset);


    fread( &info_header.size, sizeof(info_header.size), 1, bmpfile);
    fread( &info_header.width, sizeof(info_header.width), 1, bmpfile);
    fread( &info_header.height, sizeof(info_header.height), 1, bmpfile);
    fread( &info_header.planes, sizeof(info_header.planes), 1, bmpfile);
    fread( &info_header.bit_count, sizeof(info_header.bit_count), 1, bmpfile);
    fread( &info_header.compression, sizeof(info_header.compression), 1, bmpfile);
    fread( &info_header.size_img, sizeof(info_header.size_img), 1, bmpfile);

    printf("Width %d, height %d, bit_count %d\n",
	    info_header.width, info_header.height, info_header.bit_count);
    //printf("Image size %d\n", info_header.size_img);

    if (24 != info_header.bit_count || 0x4d42 != bmp_header.type)
    {
	fprintf(stderr, "The bmp file isn't RGB24 format");
	return -1;
    }
    *p_width = info_header.width;
    *p_height = info_header.height;
    *p_offset = bmp_header.offset;
    return 0;
}
    
int yuv2yv16(unsigned char *py, int width, int height, unsigned char interlaced)
{
    unsigned char *puv0, *puv1;
    unsigned char *dest;
    int bias, i, j;
    unsigned char * tmp_u;
    bias = 1;

    if (NULL == py)
	return -1;

    tmp_u = malloc(height * width / 2);
    if (NULL == tmp_u)
	return -1;

    dest = tmp_u;

    /*U*/
    for ( i = 0; i < (height); i++)
    {
	/* yuv888:
	 * u00 u01 u02
	 * u10 u11 u12
	 * yuv420:
	 * u0      u2
	 * u0 = (u00 + u01 + u10 + u11) >> 2
	 */
	puv0 = py + width * height + width * i;
	for ( j = 0; j < (width>>1); j++)
	{
	    *dest = (puv0[0] + puv0[1] + bias) >> 1;
	    dest += 1;
	    puv0 += 2;
	}
    }

    /*v*/
    dest = py + width * height;
    for ( i = 0; i < (height); i++)
    {
	/* yuv888:
	 * u00 u01 u02
	 * u10 u11 u12
	 * yuv420:
	 * u0      u2
	 * u0 = (u00 + u01 + u10 + u11) >> 2
	 */

	puv0 = py + width * height * 2 + width * i;
	for ( j = 0; j < (width>>1); j++)
	{
	    *dest = (puv0[0] + puv0[1] + bias) >> 1;
	    dest += 1;
	    puv0 += 2;
	}
    }

    dest = py + width * height * 3 / 2;
    memcpy(dest, tmp_u, width * height / 2);
    free(tmp_u);
    return 0;
} 

int yuv2yuv422p(unsigned char *py, int width, int height, unsigned char interlaced)
{
    unsigned char *puv0, *puv1;
    unsigned char *dest;
    int bias, i, j;
    unsigned char * tmp_u, *tmp_v;
    bias = 1;

    if (NULL == py)
	return -1;

    tmp_u = malloc(height * width / 2);
    tmp_v = malloc(height * width / 2);

    if ((NULL == tmp_u) || (NULL == tmp_v))
	return -1;

    dest = tmp_u;

    /*U*/
    for ( i = 0; i < (height); i++)
    {
	/* yuv888:
	 * u00 u01 u02
	 * u10 u11 u12
	 * yuv420:
	 * u0      u2
	 * u0 = (u00 + u01 + u10 + u11) >> 2
	 */
	puv0 = py + width * height + width * i;
	for ( j = 0; j < (width>>1); j++)
	{
	    *dest = (puv0[0] + puv0[1] + bias) >> 1;
	    dest += 1;
	    puv0 += 2;
	}
    }

    /*v*/
    dest = tmp_v;
    for ( i = 0; i < (height); i++)
    {
	/* yuv888:
	 * u00 u01 u02
	 * u10 u11 u12
	 * yuv420:
	 * u0      u2
	 * u0 = (u00 + u01 + u10 + u11) >> 2
	 */

	puv0 = py + width * height * 2 + width * i;
	for ( j = 0; j < (width>>1); j++)
	{
	    *dest = (puv0[0] + puv0[1] + bias) >> 1;
	    dest += 1;
	    puv0 += 2;
	}
    }

    dest = py + width * height;
    memcpy(dest, tmp_u, width * height / 2);

    dest = py + width * height*3/2;
    memcpy(dest, tmp_v, width * height / 2);

    free(tmp_u);
    free(tmp_v);

    return 0;
}

int yuv2nv12(unsigned char *py, int width, int height, unsigned char interlaced)
{
    unsigned char *puv0, *puv1;
    unsigned char *dest;
    int bias, i, j;
    bias = 1;

    if (NULL == py)
	return -1;

    dest = py + width * height;

    /*U*/
    for ( i = 0; i < (height>>1); i++)
    {
	/* yuv888:
	 * u00 u01 u02
	 * u10 u11 u12
	 * yuv420:
	 * u0      u2
	 * u0 = (u00 + u01 + u10 + u11) >> 2
	 */

	puv0 = py + width * height + width * i * 2;
	puv1 = puv0 + width;
	for ( j = 0; j < (width>>1); j++)
	{
	    *dest = (puv0[0] + puv0[1] + puv1[0] + puv1[1] + bias) >> 2;
	    bias = 3 - bias;	

	    dest += 2;
	    puv0 += 2;
	    puv1 += 2;
	}
    }

    /*V*/
    dest = py + width * height + 1;
    bias = 1;

    /*U*/
    for ( i = 0; i < (height>>1); i++)
    {
	/* yuv888:
	 * u00 u01 u02
	 * u10 u11 u12
	 * yuv420:
	 * u0      u2
	 * u0 = (u00 + u01 + u10 + u11) >> 2
	 */
	puv0 = py + 2 * width * height + width * i * 2;
	puv1 = puv0 + width;
	for ( j = 0; j < (width>>1); j++)
	{
	    *dest = (puv0[0] + puv0[1] + puv1[0] + puv1[1] + bias) >> 2;
	    bias = 3 - bias;	

	    dest += 2;
	    puv0 += 2;
	    puv1 += 2;
	}
    }

    return 0;
} 

/* ptr_input_bmp: input bmp filename
 * ptr_out_yuv: out yuv filename*/
int bmp2yuv(FILE *input_file, FILE *output_file, unsigned long ofourcc)
{
    unsigned int width, height, offset, img_size;
    unsigned char *py, *pu, *pv, *p_rgb;
    int i, j, rgb_stride;
    int ret;

    if (NULL == input_file)
    {
	fprintf(stderr, "Can't open bmp file!\n");
	return -1;
    }

    if (NULL == output_file)
    {
	fprintf(stderr, "Can't open output yuv file!\n");
	return -1;
    }

    if ( 0 != bmp2yuv_read_bmp_file(input_file, &width, &height, &offset))
    {
	 fprintf(stderr, "Read BMP Heaser fail!\n");
	 return -1;
    }

    py = malloc(width * height * 3 + 16);
    if (NULL == py)
    {
	fprintf(stderr, "malloc %d fail!\n", width * height * 3);
	return -1;
    }

    /*Every line is aligned to 4 bytes*/
    rgb_stride = (width * 3 + 3) / 4 * 4;
    //printf("RGB stride %d\n", rgb_stride);
    p_rgb = malloc(rgb_stride);

    if (NULL == p_rgb)
    {
	fprintf(stderr, "malloc %d bytes fail!\n", rgb_stride);
	return -1;
    }

    pu = py + width * height;
    pv = pu + width * height;

    fseek(input_file, offset, SEEK_SET);

    /*BMP file start from the bottom line of the image*/
    for ( i = height - 1; i >= 0 ; i--)
    {
	if (rgb_stride != fread(p_rgb, 1, rgb_stride, input_file))
	{
	    fprintf(stderr, "Read BMP file fail\n");
	    free(py);
	    free(p_rgb);
	    fclose(input_file);
	    fclose(output_file);
	    return -1;
	}   
	/* Order: BGR*/
	for ( j = 0; j < width; j++)
	{
	    unsigned char r, g, b;
	    r = p_rgb[j*3 + 2];
	    g = p_rgb[j*3 + 1];
	    b = p_rgb[j*3 + 0];
	    py[i*width + j]   = (unsigned char )((0.299f * (float)r) + 
		    (0.587f * (float)g) + 
		    (0.114f * (float)b));
	    pu[i*width + j] = (unsigned char)(-(0.169 * (float)r) - 
		    (0.332 * (float)g) + 
		    (0.5 * (float)b) + 128);

	    pv[i*width + j]  = (unsigned char)((0.5 * (float)r) - 
		    (0.419 * (float)g) - 
		    (0.0813 * (float)b) + 128); 
	}
    }

    switch (ofourcc)
    {
	case FOURCC_NV12:
	   img_size = width * height * 3 / 2;
	   ret = yuv2nv12(py, width, height, 0);
	   break;
	case FOURCC_YV16:
	   img_size = width * height * 2;
	   ret = yuv2yv16(py, width, height, 0);
	   break;
        case FOURCC_YUV422P:
           img_size = width * height * 2;
           ret = yuv2yuv422p(py, width, height, 0);
           break;
       case FOURCC_IYUV:
           img_size = width * height * 3 / 2;
           ret = 0;
    }	 
    if (ret != 0)
    {
	 fprintf(stderr, " %s L%d error!\n", __FUNCTION__, __LINE__);
    }

    ret = fwrite(py, 1, img_size, output_file);
    if (ret != img_size)
    {
	fprintf(stderr, "fwiter error! %d bytes were written\n", ret);
	ret = -1;
    }
    free(py);
    free(p_rgb);
    fclose(input_file);
    fclose(output_file);
    return ret;
}
