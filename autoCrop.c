/*
Copyright(c)2008 Internet Archive. Software license GPL version 2.

build leptonica first:
cd leptonlib-1.56/
./configure
make
cd src/
make
cd ../prog/
make

compile with:
g++ -ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3  -Ileptonlib-1.56/src -I/usr/X11R6/include  -DL_LITTLE_ENDIAN -o autoCrop autoCrop.c leptonlib-1.56/lib/nodebug/liblept.a -ltiff -ljpeg -lpng -lz -lm

run with:
autoCrop filein.jpg rotateDirection
*/

#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"
#include <assert.h>

#define debugstr printf
//#define debugstr

typedef unsigned int    UINT;
typedef unsigned int    UINT32;

static const l_float32  kInitialSweepAngle = 1.0;

int main(int    argc,
     char **argv)
{

    PIX         *pixs, *pixd, *pixg, *pixb;
    char        *filein, *fileout;
    static char  mainName[] = "autoCrop";
    l_float32    scale;
    l_int32      rotDir;
    FILE        *fp;
    FILE        *fpOut;
    int          lum, threshold;
    l_float32    angle, conf;
    
    if (argc != 3) {
        exit(ERROR_INT(" Syntax:  autoCrop filein.jpg rotateDirection",
                         mainName, 1));
    }
    
    filein  = argv[1];
    rotDir  = atoi(argv[2]);
    
    if ((fp = fopenReadStream(filein)) == NULL) {
        exit(ERROR_INT("image file not found", mainName, 1));
    }
    debugstr("Opened file handle\n");
    
    if ((pixs = pixReadStreamJpeg(fp, 0, 8, NULL, 0)) == NULL) {
       exit(ERROR_INT("pixs not made", mainName, 1));
    }
    debugstr("Read jpeg\n");

    if (rotDir) {
        pixd = pixRotate90(pixs, rotDir);
        debugstr("Rotated 90 degrees\n");
    } else {
        pixd = pixs;
    }


    /* convert color image to grayscale: */
    pixg = pixConvertRGBToGray (pixd, 0.30, 0.60, 0.10);
    //pixWrite("/home/rkumar/public_html/outgrey.jpg", pixg, IFF_JFIF_JPEG); 
    debugstr("Converted to gray\n");

    int w = pixGetWidth( pixd );
    int h = pixGetHeight( pixd );

    l_int32 retval;
    l_uint32 val;
    UINT i, j;
    l_uint32 a,b;
    l_uint32 acc=0;
    
    l_int32   wpl;
    l_uint32  *line, *data;
    wpl = pixGetWpl(pixg);
    data = pixGetData(pixg);

    l_uint32 maxDiff=0;
    l_int32 maxj=-1;
    l_float32 currentBestAngle = 0.0;
    
    for (j=0;j<(h*0.25);j++) {
        line = data + j * wpl;
        //printf("%x: ", line);
        printf("%d: ", j);
        acc=0;
        for (i=0; i<w; i++) {

   
            retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i, j+1, &b);
            assert(0 == retval);            
            //printf("%d ", val);
            acc += (abs(a-b));
        }
        printf("%d \n", acc);
        if (acc > maxDiff) {
            maxj=j;   
            maxDiff = acc;
        }
    }
    
    if (-1 == maxj) {
        printf("maxj calculation failed!\n");
    } else {
        printf("maxj = %d with diff %d\n", maxj, maxDiff);
    }
    
     l_float32   deg2rad = 3.1415926535 / 180.;
    l_float32 delta = kInitialSweepAngle;
    //check +/- delta
    PIX *pixt = pixCreateTemplate(pixg);
    pixVShearCenter(pixt,
                    pixg,
                    deg2rad*delta,
                    L_BRING_IN_BLACK);
    pixWrite("/home/rkumar/public_html/outgrey.jpg", pixt, IFF_JFIF_JPEG); 

    /*don't free this stuff.. it will be dealloced with the program terminates    
    pixDestroy(&pixs);
    pixDestroy(&pixd);
    pixDestroy(&pixg);
    pixDestroy(&pixb);
    fclose(fp);
    fclose(fpOut);*/
    
    return 0;

}

l_uint32 calculateSumOfAbsDiff(PIX *pixg, l_uint32 w, l_uint32 h) {

    UINT i, j;
    l_uint32 acc=0;
    l_uint32 a,b;
    l_uint32 maxDiff=0;
    l_int32 maxj=-1;
    
    for (j=0;j<(h*0.25);j++) {
        printf("%d: ", j);
        acc=0;
        for (i=0; i<w; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i, j+1, &b);
            assert(0 == retval);            
            //printf("%d ", val);
            acc += (abs(a-b));
        }
        printf("%d \n", acc);
        if (acc > maxDiff) {
            maxj=j;   
            maxDiff = acc;
        }
        
    }
    return maxDiff;
}
