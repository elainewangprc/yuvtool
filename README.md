This yuvtool is to display/convert yuv files.

The source code is copied from https://cgit.freedesktop.org/~AustinYuan/libmix-test/tree/yuvtool with some modification.

yuvtool <display|convert|psnr|ssim|create|rotate|md5> <options>
   for display, options is: -s <widthxheight> -i <input YUV file> -ifourcc <input fourcc> -path <output path>
   for create,  options is: -s <widthxheight> -n <frame number> -i <input YUV file> -ifourcc <input fourcc>
   for convert, options is: -s <widthxheight> -i <input YUV file> -ifourcc <input fourcc>
                                              -o <output YUV file> -ofourcc <output fourcc>
                currently, support NV12<->I420/YV12 BMP->NV12 conversion
   for psnr,    options is: -s <widthxheight> -i <input YUV file> -o <output YUV file> -n <frame number> -e
                The two files should be same with width same FOURCC and resolution
                -e will calculate each frame psnr
   for ssim,    options is: -s <widthxheight> -i <reference YUV file> -o <reconstructed YUV file> -n <frame number>
   for md5,     options is: -s <widthxheight> -i <reference YUV file> -ifourcc <input fourcc>
                Calculate the MD5 of each frame for static frame analyze
   for crc, options is:-s widthxheight  -i <input YUV file>
   for scale,   options is:-s widthxheight  -i <input YUV file> -ifourcc <input fourcc>
                           -S widthxheight  -o <output YUV file>
   for rotate,   options is:-s widthxheight  -i <input YUV file> -ifourcc <input fourcc>
                            -d <90|270> -o <output YUV file>
FOURCC could be YV12,NV12,I420,IYUV,YV16,YUV422P
Path could be xv, x11

