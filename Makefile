all: yuvtool text2yuv

srcs=bmp2yuv.c  main.c utils/psnr.c utils/ssim.c
cflags=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

yuvtool: $(srcs)
	gcc -o yuvtool -g -Wall $(srcs) $(cflags) -I. -lXv -lm -lX11 -lXext

text2yuv: text2yuv.c
	gcc -o text2yuv text2yuv.c

clean:
	rm -rf *.o yuvtool text2yuv
