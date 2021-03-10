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
 *
 *
 * Authors:
 *    Chang, Ying<ying.chang@intel.com>
 *    Lin, Edward <edward.lin@intel.com>
 *    Liu, Bolun<bolun.liu@intel.com>
 *    Sun, Jing <jing.a.sun@intel.com>
 *    Wang, Elaine<elaine.wang@intel.com>
 *    Yuan, Shengquan<shengquan.yuan@intel.com>
 *    Zhang, Zhangfei<zhangfei.zhang@intel.com>
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

#include <utils/va_wrapper.h>
#include <utils/loadsurface_yuv.h>

extern int bmp2yuv(FILE *input_bmp, FILE *out_yuv, unsigned long ofourcc);
int scale_2dimage(unsigned char *src_img, int src_imgw, int src_imgh,
                  unsigned char *dst_img, int dst_imgw, int dst_imgh);

int calc_PSNR(FILE *srcyuv_fp, FILE *recyuv_fp, int frame_width, int frame_height, int frame_count,
              double *psnr, double *psnr_y, double *psnr_u, double *psnr_v, double *mse);
int calc_SSIM(FILE *srcyuv_fp, FILE *destyuv_fp, int frame_width, int frame_height, int frame_count,
              double *ssim);


int width=0, height=0, dst_width=0, dst_height=0; /* dimension of YUV frame */
static char input_fn[256],output_fn[256];/* input,output file name */
static FILE *input_fp=NULL,*output_fp=NULL;/* FILE point of input/output file */
static char open_mode[8];
enum output_path {xv, x11} path = x11;
static unsigned long ifourcc=VA_FOURCC_YV12,ofourcc=VA_FOURCC_YV12;
static int frame_rate=30,frame_num=1;
static struct timeval tftarget;
static int output_psnr_file = 0;
static int rotate_degree = 90;
static int psnr_each_frame = 0;

static int GetPortId(Display *dpy)
{
    int i, j, k, numImages, portNum;
    XvImageFormatValues *formats;
    XvAdaptorInfo *info;
    unsigned int numAdapt;
    
    portNum = -1;
 
    if(Success != XvQueryAdaptors(dpy,DefaultRootWindow(dpy),&numAdapt,&info))
        return -1;

    fprintf(stderr, "Found %i Xv adaptors\n", numAdapt);

    for(i = 0; i < numAdapt; i++) {
        if(info[i].type & XvImageMask) {  
            /* Adaptor has XvImage support */
            formats = XvListImageFormats(dpy, info[i].base_id, &numImages);

            for(j = 0; j < numImages; j++) {
                if(formats[j].id == VA_FOURCC_YV12) {
                    /* It supports our format */
                    for(k = 0; k < info[i].num_ports; k++) {
                        /* try to grab a port */
                        if(Success == XvGrabPort(dpy, info[i].base_id + k, 
                                                 CurrentTime))
                        {
                            portNum = info[i].base_id + k;
                            break;
                        }
                    }
                }
                if(portNum != -1) break;
            }
            XFree(formats);
        }
        if(portNum != -1) break;
    }

    XvFreeAdaptorInfo(info);

    return portNum;
}

static void doframerate(void)
{
    struct timeval tfdiff;

    /* Compute desired frame rate */
    if (frame_rate <= 0)
        return;

    tftarget.tv_usec += 1000000 / frame_rate;
    /* this is where we should be */
    if (tftarget.tv_usec >= 1000000)
    {
        tftarget.tv_usec -= 1000000;
        tftarget.tv_sec++;
    }

    /* this is where we are */
    gettimeofday(&tfdiff,(struct timezone *)NULL);

    tfdiff.tv_usec = tftarget.tv_usec - tfdiff.tv_usec;
    tfdiff.tv_sec  = tftarget.tv_sec  - tfdiff.tv_sec;
    if (tfdiff.tv_usec < 0)
    {
        tfdiff.tv_usec += 1000000;
        tfdiff.tv_sec--;
    }

    /* See if we are already lagging behind */
    if (tfdiff.tv_sec < 0 || (tfdiff.tv_sec == 0 && tfdiff.tv_usec <= 0))
        return;

    /* Spin for awhile */
    select(0,NULL,NULL,NULL,&tfdiff);
}

static unsigned int mask2shift(unsigned int mask)
{
    unsigned int shift = 0;
    while((mask & 0x1) == 0)
    {
        mask = mask >> 1;
        shift++;
    }
    return shift;
}

static int display_yuv_x11(void)
{
    GC context;
    Display* dpy;
    int win_width, win_height;
    Window win;
    XImage *ximg = NULL;
    Visual *visual;
    XEvent event;
    int depth;
    int x,y;
    int i;

    unsigned char* src_y = (unsigned char*)malloc(width * height);
    unsigned char* src_u = (unsigned char*)malloc(width/2 * height/2);
    unsigned char* src_v = (unsigned char*)malloc(width/2 * height/2);
    unsigned char* y_start = src_y;
    unsigned char* u_start = src_u;
    unsigned char* v_start = src_v;
    unsigned char uv_buf[4096];/* it is enough for 1920x1080 */

    unsigned int rmask;
    unsigned int gmask;
    unsigned int bmask;
    
    unsigned int rshift;
    unsigned int gshift;
    unsigned int bshift;

    static int framecnt_tmp = 0;
    int framestop_cancel = 0;
    
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "Can't connect X server!\n");
        exit(-1);
    }

    void yuv2pixel(unsigned int *pixel, int y, int u, int v)
    {
        int r, g, b;
        /* Warning, magic values ahead */
        r = y + ((351 * (v-128)) >> 8);
        g = y - (((179 * (v-128)) + (86 * (u-128))) >> 8);
        b = y + ((444 * (u-128)) >> 8);
	
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
			
        *pixel = ((r << rshift) & rmask) | ((g << gshift) & gmask) | ((b << bshift) & bmask);
    }

    win_width = width;
    win_height = height;
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
                              0, 0, win_width, win_height, 0, 0, 0);
    XSetWindowBackgroundPixmap(dpy, win, None);
    XMapWindow(dpy, win);
    XSelectInput(dpy, win, KeyPressMask | StructureNotifyMask);
    context = XCreateGC(dpy, win, 0, 0);
    XFlush(dpy);

    visual = DefaultVisual(dpy, DefaultScreen(dpy));
    context = XCreateGC(dpy, win, 0, NULL);
    depth = DefaultDepth(dpy, DefaultScreen(dpy));

    if (TrueColor != visual->class)
    {
        fprintf(stderr, "Default visual of X display must be TrueColor.\n");
	return 0;
    }
    
    rmask = visual->red_mask;
    gmask = visual->green_mask;
    bmask = visual->blue_mask;

    rshift = mask2shift(rmask);
    gshift = mask2shift(gmask);
    bshift = mask2shift(bmask);

    ximg = XCreateImage(dpy, visual, depth, ZPixmap, 0, NULL, width, height, 32, 0 );

    if (ximg->bits_per_pixel != 32)
    {
        fprintf(stderr, "Display uses %d bits/pixel which is not supported\n", ximg->bits_per_pixel);
	return 0;
    }
    
    ximg->data = (char *) malloc(ximg->bytes_per_line * height);
    if (NULL == ximg->data) {
	fprintf(stderr, "XImage data is null.\n");
	return 0;
    }

    fprintf(stdout,"Display file %s, %dx%d, %d frames, fps=%d\n",
            input_fn,width,height,frame_num,frame_rate);
    
    gettimeofday(&tftarget,(struct timezone *)NULL);
    while (frame_num-- > 0) {
	/* reset pointers */
	src_y = y_start;
	src_u = u_start;
	src_v = v_start;

        /* copy Y plane */
        if ((ifourcc == VA_FOURCC_YV12) || (ifourcc == VA_FOURCC_NV12)
            ||(ifourcc == VA_FOURCC_I420) || (ifourcc == VA_FOURCC_IYUV)) {
	    for (i = 0; i < height; i++, src_y += width) {
	        fread(src_y, width, 1, input_fp);
	    }
	}
	/* copy UV plane */
        if ((ifourcc == VA_FOURCC_YV12)) {
            for(i = 0; i < height/2; i++, src_v += width/2) {
                fread(src_v, width/2, 1, input_fp);/* read one line of V */
            }
            for(i = 0; i < height/2; i++, src_u += width/2) {
                fread(src_u, width/2, 1, input_fp);/* read one line of V */
            }
	} else if ((ifourcc == VA_FOURCC_IYUV) || (ifourcc == VA_FOURCC_I420)) {
            for(i = 0; i < height/2; i++, src_u += width/2) {
                fread(src_u, width/2, 1, input_fp);/* read one line of U */
            }
            for(i = 0; i < height/2; i++, src_v += width/2) {
                fread(src_v, width/2, 1, input_fp);/* read one line of V */
            }
	} else if ((ifourcc == VA_FOURCC_NV12)) {
	    int j;
            for(i = 0; i < height/2; i++) {
                fread(uv_buf, width, 1, input_fp);
                for(j = 0; j < width/2; j++) {
                    src_u[j] = uv_buf[2*j];
                    src_v[j] = uv_buf[2*j+1];
                }
                src_v += width/2;
                src_u += width/2;
            }
        }
	/* reset pointers */
	src_y = y_start;
	src_u = u_start;
	src_v = v_start;

	for (y = 0; y < height; y += 2) {
	    unsigned int *dest_even = (unsigned int*) (ximg->data + y * ximg->bytes_per_line);
	    unsigned int *dest_odd = (unsigned int*) (ximg->data + (y + 1) * ximg->bytes_per_line);
	    for(x = 0; x < width; x += 2) {
		/* Y1 Y2 */
		/* Y3 Y4 */
		int y1 = *(src_y + x);
		int y2 = *(src_y + x + 1);
		int y3 = *(src_y + x + width);
                int y4 = *(src_y + x + width + 1);

                /* U V */
                int u = *(src_u + x/2);
                int v = *(src_v + x/2);
			
                yuv2pixel(dest_even++, y1, u, v);
                yuv2pixel(dest_even++, y2, u, v);

                yuv2pixel(dest_odd++, y3, u, v);
                yuv2pixel(dest_odd++, y4, u, v);
	    }
	    src_y += width * 2;
            src_u += width/2;
            src_v += width/2;
        }

        if ((framestop_cancel == 0) && getenv("FRAME_STOP")) {
            char c;
            
            fprintf(stderr, "press any key to display frame %d (c/C to continue)..\n", framecnt_tmp++);
            c = getchar();
            if (c=='c' || c=='C')
                framestop_cancel = 1;
        } else
            printf("\rDisplaying frame %d...", framecnt_tmp++);
        
        XPutImage(dpy, win, context, ximg, 0, 0, 0, 0, width, height);
        XFlush(dpy);
        while(XPending(dpy)) {
            XNextEvent(dpy, &event);

            /* rescale the video to fit the window */
            if(event.type == ConfigureNotify) { 
                win_width = event.xconfigure.width;
                win_height = event.xconfigure.height;
            }	
        }	
        doframerate();
    }
    printf("\n");
    if (getenv("FRAME_STOP")) {
        fprintf(stderr, "press any key to exit..\n");
        getchar();
    }
    if (NULL != ximg)
        XDestroyImage(ximg);
    if (NULL != y_start)
	free(y_start);
    if (NULL != u_start)
	free(u_start);
    if (NULL != v_start)
	free(v_start);

    XFreeGC(dpy, context);
    XCloseDisplay(dpy);
    return 0;
}

static int display_yuv_xv(void)
{
    Display *dpy;
    int portNum = -1,i,win_width,win_height;
    XvImage *image;
    XShmSegmentInfo shminfo;
    Window win;
    GC context;
    unsigned char *img_U,*img_V, *img_Y;
    XEvent event;

    static int framecnt_tmp = 0;
    
    dpy = XOpenDisplay(":0.0");
    
    if (dpy == NULL) {
        fprintf(stderr, "Can't connect X server!\n");
        exit(-1);
    }

    /* make sure we have an adaptor that we can use */
    portNum = GetPortId(dpy);
   
    if(portNum == -1) {
        fprintf(stderr, "Couldn't find free Xv adaptor with YV12 XvImage support");
        XCloseDisplay(dpy);
        return -1;
    }

    image = XvShmCreateImage(dpy, portNum, VA_FOURCC_YV12, 0, 
                             width, height, &shminfo);

    shminfo.shmid = shmget (IPC_PRIVATE, image->data_size, IPC_CREAT|0777);
    shminfo.shmaddr = image->data = shmat (shminfo.shmid, 0, 0);
    shmctl (shminfo.shmid, IPC_RMID, 0);
    shminfo.readOnly = False;
    if(!XShmAttach (dpy, &shminfo)) {
        fprintf(stderr, "XShmAttach failed!\n");
        XFree(image);
        XCloseDisplay(dpy);
        return -1;
    }

    /* start with a window the size of the source */
    win_width = width;
    win_height = height;
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
                              0, 0, win_width, win_height, 0, 0, 0);
    XSetWindowBackgroundPixmap(dpy, win, None);
    XMapWindow(dpy, win);
    XSelectInput(dpy, win, KeyPressMask | StructureNotifyMask);
    context = XCreateGC(dpy, win, 0, 0);
    XFlush(dpy);

    fprintf(stdout,"Display file %s, %dx%d, %d frames, fps=%d\n",
            input_fn,width,height,frame_num,frame_rate);
    
    gettimeofday(&tftarget,(struct timezone *)NULL);
    while (frame_num-- > 0) {
        unsigned char uv_buf[4096];/* it is enough for 1920x1080 */
        
        /* Image is YV12 */
        img_Y = (unsigned char *)image->data + image->offsets[0];
        img_V = (unsigned char *)image->data + image->offsets[1];
        img_U = (unsigned char *)image->data + image->offsets[2];
    
        /* copy Y plane */
        if ((ifourcc == VA_FOURCC_YV12) || (ifourcc == VA_FOURCC_NV12)
            ||(ifourcc == VA_FOURCC_I420) || (ifourcc == VA_FOURCC_IYUV)) {
            for(i = 0; i < height; i++, img_Y += image->pitches[0]) {
                fread(img_Y,width,1,input_fp);/* read one line of Y */
            }
        }

        if ((ifourcc == VA_FOURCC_YV12)) {
            for(i = 0; i < height/2;i++, img_V += image->pitches[1]) {
                fread(img_V,width/2,1,input_fp);/* read one line of V */
            }
            for(i = 0; i < height/2;i++, img_U += image->pitches[2]) {
                fread(img_U,width/2,1,input_fp);/* read one line of V */
            }
        } else if ((ifourcc == VA_FOURCC_IYUV) || (ifourcc == VA_FOURCC_I420)) {
            for(i = 0; i < height/2;i++, img_U += image->pitches[2]) {
                fread(img_U,width/2,1,input_fp);/* read one line of U */
            }
            for(i = 0; i < height/2;i++, img_V += image->pitches[1]) {
                fread(img_V,width/2,1,input_fp);/* read one line of V */
            }
        } else if ((ifourcc == VA_FOURCC_NV12)) {
            int j;
            
            for(i = 0; i < height/2; i++) {
                fread(uv_buf,width,1,input_fp);
                
                for(j = 0; j < width/2; j++) {
                    img_U[j] = uv_buf[2*j];
                    img_V[j] = uv_buf[2*j+1];
                }
                
                img_V += image->pitches[1];
                img_U += image->pitches[2];
            }
        }

        if (getenv("FRAME_STOP")) {
            fprintf(stderr, "press any key to display frame %d..\n", framecnt_tmp++);
            getchar();
        }
        
        XvShmPutImage(dpy, portNum, win, context, image, 0, 0, 
                      width, height, 0, 0, win_width, win_height, False);
        XSync(dpy, False);
        
        while(XPending(dpy)) {
            XNextEvent(dpy, &event);

            /* rescale the video to fit the window */
            if(event.type == ConfigureNotify) { 
                win_width = event.xconfigure.width;
                win_height = event.xconfigure.height;
            }	
        }	
        doframerate();
    }
    
    XShmDetach (dpy, &shminfo); 
    XCloseDisplay(dpy);
    shmdt (shminfo.shmaddr);
    XFree(image);

    return 0;
}

static int display_yuv()
{
    if (path == xv)
	display_yuv_xv();
    if (path == x11)
	display_yuv_x11();
    return 0; 
}

static char * fourcc2string(unsigned long fourcc)
{
    if (fourcc == VA_FOURCC_NV12) return "NV12";
    if (fourcc == VA_FOURCC_YV12) return "YV12";
    if (fourcc == VA_FOURCC_IYUV) return "IYUV";
    if (fourcc == VA_FOURCC_I420) return "I420";
    return "Unsupported";
}

static int convert_yuv(void)
{
    unsigned buf[4096];
    int tmp=0,read_size,i=0;

    fprintf(stdout,"Convert source file %s (%s) --> %s (%s), %dx%d, %d frames\n",
            input_fn,fourcc2string(ifourcc),output_fn,fourcc2string(ofourcc),
            width,height,frame_num);
    fprintf(stdout,"Press any key to continue.\n");
    getchar();
    
    if (ifourcc == ofourcc) {
        while ((read_size=read(fileno(input_fp),buf,4096)) != 0)
            write(fileno(output_fp),buf,read_size);
        return 0;
    } else if ((ifourcc == VA_FOURCC_NV12) &&
               ((ofourcc == VA_FOURCC_I420) || (ofourcc == VA_FOURCC_IYUV) || (ofourcc == VA_FOURCC_YV12))) {
        unsigned char *one_frame = malloc(width*height*1.5);
        unsigned char *u_plane = malloc(width*height/4);
        unsigned char *v_plane = malloc(width*height/4);
        
        while (frame_num-- > 0) {
            fprintf(stderr,"    Convert frame %i...\n",tmp++);
            
            fread(one_frame,width*height*1.5,1,input_fp);
            fwrite(one_frame,width*height,1,output_fp);/* write Y plane */
            
            for(i = 0; i < height/2; i++) {
                unsigned char *u_ptr = u_plane + i * (width/2);
                unsigned char *v_ptr = v_plane + i * (width/2);
                unsigned char *uv_ptr = one_frame + width*height + i * width;
                int j;
                
                for(j = 0; j < width/2; j++) {
                    u_ptr[j] = uv_ptr[j*2];
                    v_ptr[j] = uv_ptr[j*2+1];
                }
            }
            
            if ((ofourcc == VA_FOURCC_I420) || (ofourcc == VA_FOURCC_IYUV)) {
                fwrite(u_plane,width*height/4,1,output_fp);
                fwrite(v_plane,width*height/4,1,output_fp);
            } else if (ofourcc == VA_FOURCC_YV12) {
                fwrite(v_plane,width*height/4,1,output_fp);
                fwrite(u_plane,width*height/4,1,output_fp);
            }
        }
        free(one_frame);
        free(u_plane);
        free(v_plane);
    } else if((ifourcc == VA_FOURCC_YV12 || ifourcc == VA_FOURCC_I420 || ifourcc == VA_FOURCC_IYUV ) && (ofourcc == VA_FOURCC_NV12)) {
        unsigned char *one_frame = malloc(width*height*1.5);
        unsigned char *uv_plane = malloc(width*height/2);
        
        while (frame_num-- > 0) {
            fprintf(stderr,"    Convert frame %i...\n",tmp++);
            
            fread(one_frame,width*height*1.5,1,input_fp);
            fwrite(one_frame,width*height,1,output_fp);/* write Y plane */
            
            for(i = 0; i < height/4; i++) {
                unsigned char *uv_ptr = uv_plane + i * width*2;
                unsigned char *v_ptr; 
		unsigned char *u_ptr;
                int j;
                
		if (ifourcc == VA_FOURCC_YV12) {
		    v_ptr = one_frame + width*height + i * width;
		    u_ptr = one_frame + width*height + width*height/4 + i * width;
		} else { //I420
		    u_ptr = one_frame + width*height + i * width;
		    v_ptr = one_frame + width*height + width*height/4 + i * width;
		}

		
                for(j = 0; j < width; j++) {
                    uv_ptr[2*j] = u_ptr[j];
                    uv_ptr[2*j+1] = v_ptr[j];
                }
            }
            fwrite(uv_plane,width*height/2,1,output_fp);
        }
        free(one_frame);
        free(uv_plane);
    } else {
        fprintf(stderr,"Conversion not supported yet\n");
        return -1;
    }

    printf("Conversion done.\n");
    
    return 0;
}

static int rotate90_yuv(unsigned char *des, unsigned char *src, int width, int height)
{
    int i=0,j=0,n=0; 
    int hw=width/2,hh=height/2; 

    for(i=0; i<width; i++) 
        for(j=height-1;j>=0;j--) 
        { 
            des[n++] = src[width*j+i]; 
        } 

    unsigned char *ptmp = src+width*height; 
    if (ifourcc == VA_FOURCC_NV12) {
        for(i=0; i<=width-2; i+=2) 
            for(j=hh-1;j>=0;j--) 
            { 
                des[n++] = ptmp[width*j+i]; 
                des[n++] = ptmp[width*j+i+1]; 
            } 

    }else if(ifourcc == VA_FOURCC_IYUV){
        for(j=0;j<=hw-1;j++) 
            for(i=hh-1;i>=0;i--) 
            { 
                des[n++] = ptmp[hw*i+j]; 
            } 

        ptmp = src+width*height*5/4; 
        for(j=0;j<=hw-1;j++) 
            for(i=hh-1;i>=0;i--) 
            { 
                des[n++] = ptmp[hw*i+j]; 
            }
    }

    return 0;
}

static int rotate270_yuv(unsigned char *des, unsigned char *src, int width, int height)
{
    int i=0,j=0,n=0; 
    int hw=width/2,hh=height/2; 

    for(i=width-1; i>=0; i--) 
        for(j=0;j<height;j++) 
        { 
            des[n++] = src[width*j+i]; 
        } 

    unsigned char *ptmp = src+width*height; 
    if (ifourcc == VA_FOURCC_NV12) {
        for(i=width-2; i>=0; i-=2) 
            for(j=0;j<hh;j++) 
            { 
                des[n++] = ptmp[width*j+i]; 
                des[n++] = ptmp[width*j+i+1]; 
            } 

    }else if(ifourcc == VA_FOURCC_IYUV){
        for(j=hw-1;j>=0;j--) 
            for(i=0;i<hh;i++) 
            { 
                des[n++] = ptmp[hw*i+j]; 
            } 

        ptmp = src+width*height*5/4; 
        for(j=hw-1;j>=0;j--) 
            for(i=0;i<hh;i++) 
            { 
                des[n++] = ptmp[hw*i+j]; 
            }
    }

    return 0;
}


static int rotate_yuv(void)
{
    int tmp=0;
    
    if (ifourcc == VA_FOURCC_IYUV || ifourcc == VA_FOURCC_NV12 ) {
        unsigned char *input_frame = malloc(width*height*1.5);
        unsigned char *output_frame = malloc(width*height*1.5);
        
        while (frame_num-- > 0) {
            fprintf(stderr,"    Rotate frame %i %d degree...\n",tmp++, rotate_degree);
            
            memset(input_frame, 0, width*height*1.5);
            memset(output_frame, 0, width*height*1.5);

            fread(input_frame,width*height*1.5,1,input_fp);
            if (rotate_degree == 90)
                rotate90_yuv(output_frame, input_frame, width, height);
            else
                rotate270_yuv(output_frame, input_frame, width, height);
            fwrite(output_frame,width*height*1.5,1,output_fp);/* write Y plane */

        }
        free(input_frame);
        free(output_frame);
    } else {
        printf("Not implementated, Use IYUV fourcc\n");
        return 0;
    }
    
    printf("Conversion done.\n");
    
    return 0;
}


static int exit_with_help(void)
{
    printf("yuvtool <display|convert|psnr|ssim|create|rotate|md5> <options>\n");
    printf("   for display, options is: -s <widthxheight> -i <input YUV file> -ifourcc <input fourcc> -path <output path>\n");
    printf("   for create,  options is: -s <widthxheight> -n <frame number> -i <input YUV file> -ifourcc <input fourcc>\n");
    printf("   for convert, options is: -s <widthxheight> -i <input YUV file> -ifourcc <input fourcc>\n");
    printf("                                              -o <output YUV file> -ofourcc <output fourcc>\n");
    printf("                currently, support NV12<->I420/YV12 BMP->NV12 conversion\n");
    printf("   for psnr,    options is: -s <widthxheight> -i <input YUV file> -o <output YUV file> -n <frame number> -e\n");
    printf("                The two files should be same with width same FOURCC and resolution\n");
    printf("                -e will calculate each frame psnr\n");
    printf("   for ssim,    options is: -s <widthxheight> -i <reference YUV file> -o <reconstructed YUV file> -n <frame number>\n");
    printf("   for md5,     options is: -s <widthxheight> -i <reference YUV file> -ifourcc <input fourcc>\n");
    printf("                Calculate the MD5 of each frame for static frame analyze\n");
    printf("   for crc, options is:-s widthxheight  -i <input YUV file>\n");
    printf("   for scale,   options is:-s widthxheight  -i <input YUV file> -ifourcc <input fourcc> \n");
    printf("                           -S widthxheight  -o <output YUV file>\n");

    printf("   for rotate,   options is:-s widthxheight  -i <input YUV file> -ifourcc <input fourcc> \n");
    printf("                            -d <90|270> -o <output YUV file>\n");
    
    printf("FOURCC could be YV12,NV12,I420,IYUV,YV16,YUV422P\n");
    printf("Path could be xv, x11\n");

    
    printf("\n");
    
    exit(-1);
    
    return 0;
}

#define GET_FOURCC(fourcc,optarg)               \
    do {                                        \
        if (strcmp(optarg,"YV12")==0)           \
            fourcc = VA_FOURCC_YV12;               \
        else if (strcmp(optarg,"NV12")==0)      \
            fourcc = VA_FOURCC_NV12;               \
        else if (strcmp(optarg,"IYUV")==0)      \
            fourcc = VA_FOURCC_IYUV;               \
        else if (strcmp(optarg,"I420")==0)      \
            fourcc = VA_FOURCC_I420;               \
	else if (strcmp(optarg,"BMP") == 0)	\
		 fourcc = VA_FOURCC_BMP24;		\
	else if (strcmp(optarg,"YV16") == 0)	\
		 fourcc = VA_FOURCC_YV16;		\
        else if (strcmp(optarg,"YUV422P") == 0)    \
                 fourcc = VA_FOURCC_YUV422P;          \
        else exit_with_help();                  \
    } while (0)



static int psnr_yuv(void)
{
    double psnr = 0, mse = 0;
    double psnr_y, psnr_u, psnr_v;
    int i;
    
    fprintf(stdout,"Calculate PSNR %s vs %s, (%dx%d, %d frames)\n",
            input_fn, output_fn, width, height, frame_num);

    
    if (psnr_each_frame == 0) {
        calc_PSNR(input_fp, output_fp, width, height, frame_num,
                  &psnr, &psnr_y, &psnr_u, &psnr_v, &mse);

        printf("PSNR: %.2f (Y=%.2f,U=%.2f, V=%.2f)\n", psnr, psnr_y, psnr_u, psnr_v);
        return 0;
    }


    for (i=0; i<frame_num; i++) {
        psnr_y = psnr_u = psnr_v = psnr = mse = 0;
        calc_PSNR(input_fp, output_fp, width, height, 1,
                  &psnr, &psnr_y, &psnr_u, &psnr_v, &mse);
        fseek(input_fp, width*height*1.5, SEEK_CUR);
        fseek(output_fp, width*height*1.5, SEEK_CUR);
        
        printf("Frame %d: PSNR: %.2f (Y=%.2f,U=%.2f, V=%.2f)\n", i, psnr, psnr_y, psnr_u, psnr_v);

    }
    
    
    return 0;
}

static int ssim_yuv(void)
{
    double ssim;
    
    fprintf(stdout, "Calculate SSIM %s vs %s, (%dx%d, %d frames)\n",
            input_fn, output_fn, width, height, frame_num);

    calc_SSIM(input_fp, output_fp, width, height, frame_num, &ssim); 

    printf("SSIM: ssim = %.4f\n\n", ssim);
    
    return 0;
}


static int md5_yuv(void)
{
    unsigned int frame_size = width * height * 1.5;
    char tmp_template[32] = { 'm', 'd', '5', 'X','X','X','X','X','X' };
    char *tmp_fn, *one_frame, popen_cmd[64];
    FILE *tmp_fp;
    int current_frame = 0;
    
    fprintf(stdout, "Calculate each frame MD5, (%dx%d, %d frames)\n",
            width, height, frame_num);

    tmp_fn = mktemp(tmp_template);
    tmp_fp = fopen(tmp_fn, "w+");
    if (tmp_fp == NULL) {
        printf("Open temp file %s failed (%s)\n", tmp_fn, strerror(errno));
        exit(1);
    }
    
    one_frame = malloc(frame_size);
    if (one_frame == NULL) {
        printf("malloc error %s, exit\n", strerror(errno));
        exit(1);
    }

    sprintf(popen_cmd, "md5sum %s", tmp_fn);
    while (frame_num-- > 0) {
        char last_md5sum_output[256];
        char md5sum_output[256];

        FILE *popen_fp;
        int i;
        
        fread(one_frame, frame_size, 1, input_fp);

        rewind(tmp_fp);
        fwrite(one_frame, frame_size, 1, tmp_fp);
        fflush(tmp_fp);
        fsync(fileno(tmp_fp));

        popen_fp = popen(popen_cmd, "r");
        if (popen_fp == NULL) {
            printf("Failed to md5sum command\n" );
            exit(1);
        }
        fgets(md5sum_output, sizeof(md5sum_output), popen_fp);
        pclose(popen_fp);

        printf("Frame %d md5: ", current_frame);
        
        for (i=0; i<sizeof(md5sum_output); i++) {
            if (md5sum_output[i] == ' ')
                md5sum_output[i] = '\0';
        }
        if ((current_frame > 0) &&
            (strcmp(md5sum_output, last_md5sum_output) == 0))
            printf("***identicle***");
        else 
            printf(md5sum_output);
    
        current_frame ++;
        strcpy(last_md5sum_output, md5sum_output);
        
        printf("\n");
    }

    free(one_frame);
    fclose(tmp_fp);
    
    return 0;
}


static int scale_yuv(void)
{
    int tmp=0,i=0;
    
    unsigned char *src_yy, *src_y = malloc(width*height);
    unsigned char *src_uu, *src_u = malloc(width*height/4);
    unsigned char *src_vv, *src_v = malloc(width*height/4);

    unsigned char *dst_y = malloc(dst_width*dst_height);
    unsigned char *dst_u = malloc(dst_width*dst_height/4);
    unsigned char *dst_v = malloc(dst_width*dst_height/4);
    
    unsigned char uv_buf[4096];

    fprintf(stdout,"Scale source file %s (%s) --> %s (%s), source %dx%d, dst %dx%d, %d frames\n",
            input_fn,fourcc2string(ifourcc),output_fn,fourcc2string(ifourcc),
            width,height,dst_width, dst_height, frame_num);
    fprintf(stdout,"Press any key to continue.\n");
    getchar();

    src_yy = src_y;
    src_uu = src_u;
    src_vv = src_v;

    
    while (frame_num-- > 0) {
        fprintf(stderr,"    Scale frame %i...\n",tmp++);
    
        /* copy Y plane */
        if ((ifourcc == VA_FOURCC_YV12) || (ifourcc == VA_FOURCC_NV12)
            ||(ifourcc == VA_FOURCC_I420) || (ifourcc == VA_FOURCC_IYUV)) {
            for (i = 0; i < height; i++, src_y += width) {
                fread(src_y, width, 1, input_fp);
            }
        }
        /* copy UV plane */
        if ((ifourcc == VA_FOURCC_YV12)) {
            for(i = 0; i < height/2; i++, src_v += width/2) {
                fread(src_v, width/2, 1, input_fp);/* read one line of V */
            }
            for(i = 0; i < height/2; i++, src_u += width/2) {
                fread(src_u, width/2, 1, input_fp);/* read one line of V */
            }
        } else if ((ifourcc == VA_FOURCC_IYUV) || (ifourcc == VA_FOURCC_I420)) {
            for(i = 0; i < height/2; i++, src_u += width/2) {
                fread(src_u, width/2, 1, input_fp);/* read one line of U */
            }
            for(i = 0; i < height/2; i++, src_v += width/2) {
                fread(src_v, width/2, 1, input_fp);/* read one line of V */
            }
        } else if ((ifourcc == VA_FOURCC_NV12)) {
            int j;
            for(i = 0; i < height/2; i++) {
                fread(uv_buf, width, 1, input_fp);
                for(j = 0; j < width/2; j++) {
                    src_u[j] = uv_buf[2*j];
                    src_v[j] = uv_buf[2*j+1];
                }
                src_v += width/2;
                src_u += width/2;
            }
        }

        src_y = src_yy;
        src_u = src_uu;
        src_v = src_vv;

        /* scaling Y */
        scale_2dimage(src_y, width, height,
                      dst_y, dst_width, dst_height);
            
        /* scaling U */
        scale_2dimage(src_u, width/2, height/2,
                      dst_u, dst_width/2, dst_height/2);
        
        /* scaling V */
        scale_2dimage(src_v, width/2, height/2,
                      dst_v, dst_width/2, dst_height/2);

        if ((ifourcc == VA_FOURCC_YV12) || (ifourcc == VA_FOURCC_NV12)
            ||(ifourcc == VA_FOURCC_I420) || (ifourcc == VA_FOURCC_IYUV))
            fwrite(dst_y, dst_width * dst_height, 1, output_fp);

        if (ifourcc == VA_FOURCC_YV12) {
            fwrite(dst_v, dst_width/2 * dst_height/2, 1, output_fp);
            fwrite(dst_u, dst_width/2 * dst_height/2, 1, output_fp);
        }
        
        if ((ifourcc == VA_FOURCC_I420) || (ifourcc == VA_FOURCC_IYUV)) {
            fwrite(dst_u, dst_width/2 * dst_height/2, 1, output_fp);
            fwrite(dst_v, dst_width/2 * dst_height/2, 1, output_fp);
        }

        if (ifourcc == VA_FOURCC_NV12) {
            int i;
            for(i = 0; i < dst_width*dst_height/4; i++) {
                fwrite(dst_u + i, 1, 1, output_fp);
                fwrite(dst_v + i, 1, 1, output_fp); 
            }
        }
    }

    free(src_y);
    free(src_u);
    free(src_v);
    
    printf("Scale done.\n");
    
    return 0;
}


static int YUV_Generator_Planar(int width, int height,
                                unsigned char *Y_start, int Y_pitch,
                                unsigned char *U_start, int U_pitch,
                                unsigned char *V_start, int V_pitch,
                                unsigned int fourcc)
{
    static int row_shift = 0;
    int box_width = 8;

    if (row_shift == 16) row_shift = 0;

    yuvgen_planar(width,  height,
                  Y_start,  Y_pitch,
                  U_start,  U_pitch,
                  V_start,  V_pitch,
                  fourcc,  box_width,  row_shift,
                  0);

    row_shift++;
    
    return 0;
}


static int create_yuv(void)
{
    int i;
    unsigned char *one_frame=malloc(width*height*1.5);
    
    fprintf(stdout,"Create YUV file with fourcc %s, saved into file %s(%dx%d, %d frames)\n",
            fourcc2string(ifourcc),input_fn, width,height,frame_num);

    /* truncate the file */
    ftruncate(fileno(input_fp), 0);
        
    if ((ifourcc == VA_FOURCC_I420) || (ifourcc == VA_FOURCC_IYUV)) {
        for (i=0; i<frame_num; i++) {
            YUV_Generator_Planar(width,height,
                                 one_frame, width,
                                 one_frame + width*height, width/2,
                                 one_frame + width*height + (width/2)*(height/2), width/2,
                                 ifourcc);
            fwrite(one_frame, width*height*1.5, 1, input_fp);
        }
    } else if ((ifourcc == VA_FOURCC_YV12)) {
        for (i=0; i<frame_num; i++) {
            YUV_Generator_Planar(width,height,
                                 one_frame, width,
                                 one_frame + width*height + (width/2)*(height/2), width/2,
                                 one_frame + width*height, width/2,                                 
                                 ifourcc);
            fwrite(one_frame, width*height*1.5, 1, input_fp);
        }
    } else if (ifourcc == VA_FOURCC_NV12) {
        for (i=0; i<frame_num; i++) {
            YUV_Generator_Planar(width,height,
                                 one_frame, width,
                                 one_frame + width*height, width,
                                 one_frame + width*height, width,
                                 ifourcc);
            fwrite(one_frame, width*height*1.5, 1, input_fp);
        }
    } else {
        printf("FourCC is not supported\n");
    }

    free(one_frame);
    
    return 0;
}


static enum {
    DISPLAY=1,
    CONVERT,
    PSNR,
    SSIM,
    CREATE,
    SCALE,
    ROTATE,
    MD5
} operation;


#define MIN(a,b) ((a)>(b)?(b):(a))
    
int main(int argc, char **argv)
{
    int c;
    struct stat input_stat ={0};
    struct stat output_stat = {0};
    int input_frame_num=0;
    
    const struct option long_opts[] = {
        { "ifourcc", required_argument, NULL, 1 },
        { "ofourcc", required_argument, NULL, 2 },
        { "fps", required_argument, NULL, 3 },
	{ "path", required_argument, NULL, 4},
        { NULL, no_argument, NULL, 0 }
    };
    int long_index;

    strcpy(open_mode,"r");
    
    if (argv[1] != NULL) {
        if (strcmp(argv[1],"display") == 0) 
            operation = DISPLAY;
        else if (strcmp(argv[1],"convert") == 0)
            operation = CONVERT;
        else if (strcmp(argv[1],"psnr") == 0)
            operation = PSNR;
        else if (strcmp(argv[1],"ssim") == 0)
            operation = SSIM;
        else if (strcmp(argv[1],"create") == 0) {
            operation = CREATE;
            strcpy(open_mode,"w+");
        } else if (strcmp(argv[1],"scale") == 0) 
            operation = SCALE; 
        else if (strcmp(argv[1],"rotate") == 0) 
            operation = ROTATE;
        else if (strcmp(argv[1],"md5") == 0) 
            operation = MD5;
        else {
            printf("ERROR:The first parameter isn't <scale|display|convert|psnr|create|crc>, exit\n");
            exit_with_help();
        }
    } else exit_with_help();
   
    argc--;
    argv++;
            
    while ((c=getopt_long_only(argc,argv,"fen:s:S:i:d:o:h?",long_opts,&long_index)) != -1) {
        switch (c) {
        case 'h':
        case '?':
        case ':':
            exit_with_help();
            break;
        case 'f':
            output_psnr_file = 1;
            break;
        case 'e':
            psnr_each_frame = 1;
            break;
        case 'n':
            input_frame_num = atoi(optarg);
            break;
        case 'i':
            if ((input_fp = fopen(optarg,open_mode)) == NULL) {
                fprintf(stderr,"Open input file %s failed\n",optarg);
                exit_with_help();
            }
            strcpy(input_fn,optarg);
            break;
        case 's':
            if (sscanf(optarg,"%dx%d",&width,&height) != 2) {
                fprintf(stderr,"can not get width/height from %s\n",optarg);
                exit_with_help();
            }
            break;
        case 'S':
            if (sscanf(optarg,"%dx%d",&dst_width,&dst_height) != 2) {
                fprintf(stderr,"can not get dst width/height from %s\n",optarg);
                exit_with_help();
            }
            break;
            
        case 'o': /* ignore "-o" for other options */
            if ((operation == CONVERT || operation == SCALE || operation == ROTATE)) {
                if ((output_fp = fopen(optarg,"w+")) == NULL) {
                    fprintf(stderr,"Open output file %s failed\n",optarg);
                    exit_with_help();
                }
                strcpy(output_fn,optarg);
            } else if ((operation == PSNR) || (operation == SSIM)){
                if ((output_fp = fopen(optarg,"r")) == NULL) {
                    fprintf(stderr,"Open output file %s failed\n",optarg);
                    exit_with_help();
                }
                strcpy(output_fn,optarg);
            }
            break;
        case 'd':
            rotate_degree = atoi(optarg);
            if (rotate_degree != 90 && rotate_degree != 270) {
                printf("Invalide rotation degree %d, force to 90 degree\n", rotate_degree);
                rotate_degree = 90;
            }
            
            break;
        case 1:
            GET_FOURCC(ifourcc,optarg);
            break;
        case 2:
            if ((operation == CONVERT) || (operation == PSNR) || (operation == SSIM)) {
                GET_FOURCC(ofourcc,optarg);
            }
            break;
        case 3:
            frame_rate = atoi(optarg);
            break;
	case 4:
	    if (strcmp(optarg, "xv") == 0)
		path = xv;
	    else if (strcmp(optarg, "x11") == 0)
		path = x11;
	    else
                exit_with_help();
	    break;
        }

    }
    
    if ((width == 0) || (height == 0) || (input_fp == NULL)) {
        fprintf(stderr,"invalid parameters\n");
        exit_with_help();
    }

    if ((path != xv) && (path != x11)) {
        fprintf(stderr,"invalid parameters\n");
        exit_with_help();
    }

    if (fstat(fileno(input_fp),&input_stat) != 0){
        fprintf(stderr,"can't get input file status\n");
        exit(-1);
    }

    if (operation == CREATE) {
        frame_num = input_frame_num;
    } else {
        frame_num = input_stat.st_size/(width*height*1.5);
        if (output_fp) {
            if (fstat(fileno(output_fp),&output_stat) != 0){
                fprintf(stderr,"can't get output file status\n");
                exit(-1);
            }
            //frame_num = MIN(frame_num, output_stat.st_size/(width*height*1.5));
        }
        
        if (input_frame_num > 0)
            frame_num = MIN(input_frame_num, frame_num);
    }
    
    if ((operation == CONVERT) || (operation == PSNR) || \
        (operation == SCALE) || (operation == ROTATE)) {
        if (output_fp == NULL) {
            fprintf(stderr,"invalid parameters\n");
            exit_with_help();
        }
        if (fstat(fileno(output_fp),&output_stat) != 0){
            fprintf(stderr,"can't get output file status\n");
            exit(-1);
        }
        
        frame_num = MIN(frame_num, input_stat.st_size/(width*height*1.5));
    }
    if ((operation == CONVERT) || ((operation == ROTATE)))
        frame_num = input_stat.st_size/(width*height*1.5);

    if (operation == DISPLAY) display_yuv();
    if (operation == CREATE)  create_yuv();
    if (operation == SCALE) scale_yuv();
    if (operation == ROTATE) rotate_yuv();
    if (operation == PSNR) psnr_yuv();
    if (operation == SSIM) ssim_yuv();
    if (operation == MD5) md5_yuv();
    if (operation == CONVERT && ifourcc == VA_FOURCC_BMP24)
	bmp2yuv(input_fp, output_fp, ofourcc);
    else if (operation == CONVERT) 
	convert_yuv();
    
    fprintf(stderr, "\n");
    return 1;
    
}
