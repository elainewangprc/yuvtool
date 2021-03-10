/*
 * Copyright (c) 2007-2009 Intel Corporation. All Rights Reserved.
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


/**
 * Images and Subpictures
 * VAImage is used to either get the surface data to client memory, or 
 * to copy image data in client memory to a surface. 
 * Both images, subpictures and surfaces follow the same 2D coordinate system where origin 
 * is at the upper left corner with positive X to the right and positive Y down
 */
#define VA_FOURCC(ch0, ch1, ch2, ch3) \
    ((unsigned long)(unsigned char) (ch0) | ((unsigned long)(unsigned char) (ch1) << 8) | \
    ((unsigned long)(unsigned char) (ch2) << 16) | ((unsigned long)(unsigned char) (ch3) << 24 ))

/*
 * Pre-defined fourcc codes
 */
#define VA_FOURCC_NV12          0x3231564E
#define VA_FOURCC_NV21          0x3132564E
#define VA_FOURCC_AI44          0x34344149
#define VA_FOURCC_RGBA          0x41424752
#define VA_FOURCC_RGBX          0x58424752
#define VA_FOURCC_BGRA          0x41524742
#define VA_FOURCC_BGRX          0x58524742
#define VA_FOURCC_ARGB          0x42475241
#define VA_FOURCC_XRGB          0x42475258
#define VA_FOURCC_ABGR          0x52474241
#define VA_FOURCC_XBGR          0x52474258
#define VA_FOURCC_UYVY          0x59565955
#define VA_FOURCC_YUY2          0x32595559
#define VA_FOURCC_AYUV          0x56555941
#define VA_FOURCC_NV11          0x3131564e
#define VA_FOURCC_YV12          0x32315659
#define VA_FOURCC_P208          0x38303250
#define VA_FOURCC_IYUV          0x56555949
#define VA_FOURCC_YV24          0x34325659
#define VA_FOURCC_YV32          0x32335659
#define VA_FOURCC_Y800          0x30303859
#define VA_FOURCC_IMC3          0x33434D49
#define VA_FOURCC_411P          0x50313134
#define VA_FOURCC_422H          0x48323234
#define VA_FOURCC_422V          0x56323234
#define VA_FOURCC_444P          0x50343434
#define VA_FOURCC_RGBP          0x50424752
#define VA_FOURCC_BGRP          0x50524742
#define VA_FOURCC_411R          0x52313134 /* rotated 411P */


#define VA_FOURCC_I420          VA_FOURCC('I','4','2','0')
#define VA_FOURCC_BMP24         VA_FOURCC('B','M','P','2')
#define VA_FOURCC_YUV422P       VA_FOURCC('4','2','2','P')

/**
 * Planar YUV 4:2:2.
 * 8-bit Y plane, followed by 8-bit 2x1 subsampled V and U planes
 */
#define VA_FOURCC_YV16          0x36315659
/**
 * 10-bit and 16-bit Planar YUV 4:2:0.
 */
#define VA_FOURCC_P010          0x30313050
#define VA_FOURCC_P016          0x36313050


/** De-interlacing flags for vaPutSurface() */
#define VA_FRAME_PICTURE        0x00000000
#define VA_TOP_FIELD            0x00000001
#define VA_BOTTOM_FIELD         0x00000002
