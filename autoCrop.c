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
#include <math.h>

#define debugstr printf
//#define debugstr

typedef unsigned int    UINT;
typedef unsigned int    UINT32;

static const l_float32  kInitialSweepAngle = 1.0;
static const l_float32  deg2rad            = 3.1415926535 / 180.;
l_uint32 calculateSADrow(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 top, l_uint32 bottom, l_int32 *retj, l_uint32 *retDiff);
l_uint32 calculateSADcol(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 left, l_uint32 right, l_int32 *reti, l_uint32 *retDiff);
l_uint32 calcLimitLeft(l_uint32 w, l_uint32 h, l_float32 angle);
l_uint32 calcLimitTop(l_uint32 w, l_uint32 h, l_float32 angle);

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

    printf("W=%d\n", pixGetWidth( pixd ));
    printf("H=%d\n",  pixGetHeight( pixd ));


    /* convert color image to grayscale: */
    pixg = pixConvertRGBToGray (pixd, 0.30, 0.60, 0.10);
    pixWrite("/home/rkumar/public_html/outgrey.jpg", pixg, IFF_JFIF_JPEG); 
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
    l_int32 maxj=-1, maxi=-1;
    l_float32 currentBestAngle = 0.0;
    


    l_float32 delta = -0.1;
    //check +/- delta
 
    l_int32   cropTop=-1, cropBottom=-1, cropLeft=-1, cropRight=-1;
    l_uint32  topDiff = 0, bottomDiff = 0, leftDiff=0, rightDiff=0;
    l_float32 topSkew, bottomSkew, leftSkew, rightSkew;

    for (delta=-1.0; delta<=1.0; delta+=0.05) {
        /*
       //PIX *pixt = pixCreateTemplate(pixg); 
       pixVShearCenter(pixt,
                        pixg,
                        deg2rad*delta,
                        L_BRING_IN_BLACK);
        */
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);

        l_uint32 limitLeft = calcLimitLeft(w,h,delta);
        l_uint32 limitTop  = calcLimitTop(w,h,delta);

        printf("delta = %f, leftLim=%d, topLim=%d\n", delta, limitLeft, limitTop);
        //if ((0.24 < delta) && (delta < 0.26)) {
        //    pixWrite("/home/rkumar/public_html/outgrey.jpg", pixt, IFF_JFIF_JPEG); 
        //    printf("wrote outgrey for delta=%f\n", delta);
        //}
        
        calculateSADrow(pixt, w, h, limitTop, h>>2, &maxj, &maxDiff);
        //printf("top delta=%f, new maxj = %d with diff %d\n", delta, maxj, maxDiff);
        if (maxDiff>topDiff) {
            topDiff = maxDiff;
            cropTop = maxj;
            topSkew = delta;
        }

        //calculateSADrow(pixt, w, h, (int)(h*0.75), h-1, &maxj, &maxDiff);
        calculateSADrow(pixt, w, h, (int)(h*0.75), h-limitTop-1, &maxj, &maxDiff);
        //printf("bottom delta=%f, new maxj = %d with diff %d\n", delta, maxj, maxDiff);
        if (maxDiff>bottomDiff) {
            bottomDiff = maxDiff;
            cropBottom = maxj;
            bottomSkew = delta;
        }

        calculateSADcol(pixt, w, h, limitLeft, w>>2, &maxi, &maxDiff);
        if (maxDiff>leftDiff) {
            leftDiff = maxDiff;
            cropLeft = maxi;
            leftSkew = delta;
        }

        calculateSADcol(pixt, w, h, (int)(w*0.75), w-limitLeft-1, &maxi, &maxDiff);
        if (maxDiff>rightDiff) {
            rightDiff = maxDiff;
            cropRight = maxi;
            rightSkew = delta;
        }

        //pixDestroy(&pixt);
    }
    printf("cropTop    = %d (diff=%d, angle=%f)\n", cropTop, topDiff, topSkew);
    printf("cropBottom = %d (diff=%d, angle=%f)\n", cropBottom, bottomDiff, bottomSkew);
    printf("cropLeft = %d (diff=%d, angle=%f)\n", cropLeft, leftDiff, leftSkew);
    printf("cropRight = %d (diff=%d, angle=%f)\n", cropRight, rightDiff, rightSkew);

    l_float32 aveSkew = (topSkew+bottomSkew+rightSkew+leftSkew)/4;
    BOX *box = boxCreate(cropLeft, cropTop, cropRight-cropLeft, cropBottom-cropTop);
    PIX *pixCrop = pixRotate(pixd,
                    deg2rad*aveSkew,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    pixRenderBoxArb(pixCrop, box, 1, 255, 0, 0);
    pixWrite("/home/rkumar/public_html/outcrop.jpg", pixCrop, IFF_JFIF_JPEG); 

    aveSkew = (topSkew+bottomSkew)/2;
    pixCrop = pixRotate(pixd,
                    deg2rad*aveSkew,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    pixRenderBoxArb(pixCrop, box, 1, 255, 0, 0);
    pixWrite("/home/rkumar/public_html/outcrop2.jpg", pixCrop, IFF_JFIF_JPEG); 

    /*don't free this stuff.. it will be dealloced with the program terminates    
    pixDestroy(&pixs);
    pixDestroy(&pixd);
    pixDestroy(&pixg);
    pixDestroy(&pixb);
    fclose(fp);
    fclose(fpOut);*/
    
    return 0;

}


//calculate sum of absolute differences of two rows of adjacent pixels
//last SAD calculation is for row j=bottom-1 and j=bottom.
l_uint32 calculateSADrow(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 top, l_uint32 bottom, l_int32 *retj, l_uint32 *retDiff) {

    UINT i, j;
    l_uint32 acc=0;
    l_uint32 a,b;
    l_uint32 maxDiff=0;
    l_int32 maxj=-1;
    
    //printf("W=%d\n", pixGetWidth( pixg ));
    //printf("H=%d\n",  pixGetHeight( pixg ));
    assert(top>=0);
    assert(top<bottom);
    assert(bottom<h);

    for (j=top;j<bottom;j++) {
        //printf("%d: ", j);
        acc=0;
        for (i=0; i<w; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i, j+1, &b);
            assert(0 == retval);            
            //printf("%d ", val);
            acc += (abs(a-b));
        }
        //printf("%d \n", acc);
        if (acc > maxDiff) {
            maxj=j;   
            maxDiff = acc;
        }
        
    }

    *retj = maxj;
    *retDiff = maxDiff;
    return (-1 != maxj);
}

//calculate sum of absolute differences of two rows of adjacent column
//last SAD calculation is for row j=right-1 and j=right.
l_uint32 calculateSADcol(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 left, l_uint32 right, l_int32 *reti, l_uint32 *retDiff) {

    UINT i, j;
    l_uint32 acc=0;
    l_uint32 a,b;
    l_uint32 maxDiff=0;
    l_int32 maxi=-1;
    
    //printf("W=%d\n", pixGetWidth( pixg ));
    //printf("H=%d\n",  pixGetHeight( pixg ));
    assert(left>=0);
    assert(left<right);
    assert(right<w);

    for (i=left; i<right; i++) {
        //printf("%d: ", i);
        acc=0;
        for (j=0; j<h; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i+1, j, &b);
            assert(0 == retval);
            //printf("%d ", val);
            acc += (abs(a-b));
        }
        //printf("%d \n", acc);
        if (acc > maxDiff) {
            maxi=i;   
            maxDiff = acc;
        }
        
    }

    *reti = maxi;
    *retDiff = maxDiff;
    return (-1 != maxi);
}

l_uint32 calcLimitLeft(l_uint32 w, l_uint32 h, l_float32 angle) {
    l_uint32  w2 = w>>1;
    l_uint32  h2 = h>>1;
    l_float32 r  = sqrt(w2*w2 + h2*h2);
    
    l_float32 theta  = atan2(h2, w2);
    l_float32 radang = fabs(angle)*deg2rad;
    
    return w2 - (int)(r*cos(theta + radang));
}

l_uint32 calcLimitTop(l_uint32 w, l_uint32 h, l_float32 angle) {
    l_uint32  w2 = w>>1;
    l_uint32  h2 = h>>1;
    l_float32 r  = sqrt(w2*w2 + h2*h2);
    
    l_float32 theta  = atan2(h2, w2);
    l_float32 radang = fabs(angle)*deg2rad;
    
    return h2 - (int)(r*sin(theta - radang));
}
