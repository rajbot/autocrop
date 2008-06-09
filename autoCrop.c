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
#include <float.h> //for FLT_MAX

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


l_uint32 removeBlackPelsColLeft(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom);
l_uint32 removeBlackPelsColRight(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom);

l_uint32 removeBlackPelsRowTop(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 startj, l_uint32 endj, l_uint32 left, l_uint32 right);
l_uint32 removeBlackPelsRowBottom(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 top, l_uint32 oldBottom, l_uint32 left, l_uint32 right);

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
    pixWrite("/home/rkumar/public_html/out.jpg", pixd, IFF_JFIF_JPEG); 
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

        //printf("delta = %f, leftLim=%d, topLim=%d\n", delta, limitLeft, limitTop);
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
    
    PIX *pixt = pixRotate(pixg,
                    deg2rad*aveSkew,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);    

    l_float32 minVar;
    //sumCol(pixt, w, h, cropLeft+1, w>>2, cropTop, cropBottom, &maxi, &minVar);
    //printf("second left crop = %d, var = %f\n", maxi, minVar);
    //cropLeft = maxi;
    printf("adjusting cropLeft\n");
    cropLeft = removeBlackPelsColLeft(pixt, w, h, cropLeft, w>>2, cropTop, cropBottom);

    //adjustCropCol(0.20, 0.20, pixt, w, h, (int)(w*0.75), cropRight, cropTop, cropBottom, &maxi, &minVar);

    printf("adjusting cropRight\n");
    cropRight = removeBlackPelsColRight(pixt, w, h, cropRight, (int)(w*0.75), cropTop, cropBottom);
    //printf("second right crop = %d, var = %f\n", maxi, minVar);
    //cropRight = maxi;

    printf("adjusting cropTop\n");
    cropTop = removeBlackPelsRowTop(pixt, w, h, cropTop, h>>2, cropLeft, cropRight);

    printf("adjusting cropBottom\n");
    cropBottom = removeBlackPelsRowBottom(pixt, w, h, (int)(h*0.75), cropBottom, cropLeft, cropRight);
    
    BOX *box = boxCreate(cropLeft, cropTop, cropRight-cropLeft, cropBottom-cropTop);
    PIX *pixCrop = pixRotate(pixd,
                    deg2rad*aveSkew,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    pixRenderBoxArb(pixCrop, box, 1, 255, 0, 0);
    pixWrite("/home/rkumar/public_html/outbox.jpg", pixCrop, IFF_JFIF_JPEG); 
    pixCrop = pixClipRectangle(pixd, box, NULL);
    pixWrite("/home/rkumar/public_html/outcrop.jpg", pixCrop, IFF_JFIF_JPEG); 
    
    

    /*
    aveSkew = (topSkew+bottomSkew)/2;
    pixCrop = pixRotate(pixd,
                    deg2rad*aveSkew,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    pixRenderBoxArb(pixCrop, box, 1, 255, 0, 0);
    pixWrite("/home/rkumar/public_html/outcrop2.jpg", pixCrop, IFF_JFIF_JPEG); 
    */
    
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

l_uint32 removeBlackPelsColRight(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom) {
    UINT i, j;
    l_uint32 a;

    l_uint32 numBlackPels=0;
    l_uint32 blackThresh=140;

    numBlackPels = 0;
    for (j=top; j<bottom; j++) {
        l_int32 retval = pixGetPixel(pixg, starti-1, j, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }
    //printf("init: numBlack=%d\n", numBlackPels);
    
    if (numBlackPels > 10) {
        //printf(" needs adjustment!\n");
        for (i=starti-2; i>=endi; i--) {
            numBlackPels = 0;
            for (j=top; j<bottom; j++) {
                l_int32 retval = pixGetPixel(pixg, i, j, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
            //printf("%d: numBlack=%d\n", i, numBlackPels);
            if (numBlackPels<5) {
                //printf("break!\n");
                return i;
            }
        }
    }
    
    return starti;
    
}

l_uint32 removeBlackPelsColLeft(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom) {
    UINT i, j;
    l_uint32 a;

    l_uint32 numBlackPels=0;
    l_uint32 blackThresh=140;

    numBlackPels = 0;
    for (j=top; j<bottom; j++) {
        l_int32 retval = pixGetPixel(pixg, starti+1, j, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }
    //printf("init: i=%d, numBlack=%d\n", starti, numBlackPels);
    
    if (numBlackPels > 10) {
        //printf(" needs adjustment!\n");
        for (i=starti+2; i<=endi; i++) {
            numBlackPels = 0;
            for (j=top; j<bottom; j++) {
                l_int32 retval = pixGetPixel(pixg, i, j, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
            //printf("%d: numBlack=%d\n", i, numBlackPels);
            if (numBlackPels<5) {
                //printf("break!\n");
                return i;
            }
        }
    }
    
    return starti;
    
}

l_uint32 removeBlackPelsRowTop(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 startj, l_uint32 endj, l_uint32 left, l_uint32 right) {
    UINT i, j;
    l_uint32 a;

    l_uint32 numBlackPels=0;
    l_uint32 blackThresh=140;

    numBlackPels = 0;
    for (i=left; i<right; i++) {
        l_int32 retval = pixGetPixel(pixg, i, startj+1, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }
    //printf("init: j=%d, numBlack=%d\n", startj, numBlackPels);
    
    if (numBlackPels > 10) {
        //printf(" needs adjustment!\n");
        for (j=startj+2; j<=endj; j++) {
            numBlackPels = 0;
            for (i=left; i<right; i++) {
                l_int32 retval = pixGetPixel(pixg, i, j, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
            //printf("%d: numBlack=%d\n", j, numBlackPels);
            if (numBlackPels<5) {
                //printf("break!\n");
                return j;
            }
        }
    }
    
    return startj;
    
}

l_uint32 removeBlackPelsRowBottom(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 top, l_uint32 oldBottom, l_uint32 left, l_uint32 right) {
    UINT i, j;
    l_uint32 a;

    l_uint32 numBlackPels=0;
    l_uint32 blackThresh=140;

    numBlackPels = 0;
    for (i=left; i<right; i++) {
        l_int32 retval = pixGetPixel(pixg, i, oldBottom-1, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }
    //printf("init: j=%d, numBlack=%d\n", oldBottom, numBlackPels);
    
    if (numBlackPels > 10) {
        //printf(" needs adjustment!\n");
        for (j=oldBottom-2; j<=top; j++) {
            numBlackPels = 0;
            for (i=left; i<right; i++) {
                l_int32 retval = pixGetPixel(pixg, i, j, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
            //printf("%d: numBlack=%d\n", j, numBlackPels);
            if (numBlackPels<5) {
                //printf("break!\n");
                return j;
            }
        }
    }
    
    return oldBottom;
    
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

// old debugging functions to be removed later.
//______________________________________________________________________________
/*
l_uint32 calculateSADcolDebug(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 left, l_uint32 right, l_int32 *reti, l_uint32 *retDiff) {

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
        printf("%d: ", i);
        acc=0;
        for (j=0; j<h; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i+1, j, &b);
            assert(0 == retval);
            //printf("%d ", val);
            acc += (abs(a-b));
        }
        printf("%d \n", acc);
        if (acc > maxDiff) {
            maxi=i;   
            maxDiff = acc;
        }
        
    }

    *reti = maxi;
    *retDiff = maxDiff;
    return (-1 != maxi);
}
*/
/*
l_uint32 sumCol(PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 left, l_uint32 right, l_uint32 top, l_uint32 bottom, l_int32 *reti, l_float32 *retVar) {

    UINT i, j;
    l_uint32 acc=0;
    l_uint32 a;
    l_uint32 maxDiff=0;
    l_int32 minI=-1;
    
    //printf("W=%d\n", pixGetWidth( pixg ));
    //printf("H=%d\n",  pixGetHeight( pixg ));
    assert(left>=0);
    assert(left<right);
    assert(right<w);
    l_float32 minVar = FLT_MAX;
    
    for (i=left; i<right; i++) {
        printf("%d: ", i);
        acc=0;
        for (j=top; j<bottom; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            acc += a;
        }
        printf("%d ", acc);
        l_float32 ave = (float)acc/(bottom-top);
        printf("%f, ", ave);

        l_float32 var=0;
        for (j=top; j<bottom; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            l_float32 tmp = (a-ave);
            var += (tmp*tmp);
        }
        
        var = var/(bottom-top);
        printf("var=%f\n", var);

        if (var < minVar) {
            minVar = var;
            minI   = i;
        }
    }

    *reti = minI;
    *retVar = minVar;
    return (-1 != minI);
}
*/
/*
l_uint32 adjustCropCol(l_float32 thresh, l_float32 avgThresh, PIX *pixg, l_uint32 w, l_uint32 h, l_uint32 left, l_uint32 right, l_uint32 top, l_uint32 bottom, l_int32 *reti, l_float32 *retVar) {

    UINT i, j;
    l_uint32 acc=0;
    l_uint32 a;
    l_uint32 maxDiff=0;
    l_int32 minI=-1;

    l_uint32 numBlackPels;
    l_uint32 blackThresh=140;
    
    //printf("W=%d\n", pixGetWidth( pixg ));
    //printf("H=%d\n",  pixGetHeight( pixg ));
    assert(left>=0);
    assert(left<right);
    assert(right<w);
    l_float32 minVar = FLT_MAX;
    l_float32 minIavg = FLT_MAX;
    
    for (i=right; i>=left; i--) {
        printf("%d: ", i);
        acc=0;
        numBlackPels = 0;
        for (j=top; j<bottom; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            acc += a;
            if (a<blackThresh) {
                numBlackPels++;
            }
        }
        printf("numBlack=%d, ", numBlackPels);
        printf("%d ", acc);
        l_float32 ave = (float)acc/(bottom-top);
        printf("%f, ", ave);

        l_float32 var=0;
        for (j=top; j<bottom; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            l_float32 tmp = (a-ave);
            var += (tmp*tmp);
        }
        
        var = var/(bottom-top);
        printf("var=%f", var);

        if (var < (minVar*(1-thresh))) {
            if ( (ave<(minIavg*(1-avgThresh))) || (ave>(minIavg*(1>avgThresh))) ){
                minVar = var;
                minIavg = ave;
                minI   = i;
                printf(" *");
            }
        }
        printf("\n");
    }

    *reti = minI;
    *retVar = minVar;
    return (-1 != minI);
}
*/
