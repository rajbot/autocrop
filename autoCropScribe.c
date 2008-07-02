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
g++ -ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3  -Ileptonlib-1.56/src -I/usr/X11R6/include  -DL_LITTLE_ENDIAN -o autoCropScribe autoCropScribe.c leptonlib-1.56/lib/nodebug/liblept.a -ltiff -ljpeg -lpng -lz -lm

run with:
autoCropScribe filein.jpg rotateDirection
*/

#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"
#include <assert.h>
#include <math.h>   //for sqrt

#define debugstr printf
//#define debugstr

static const l_float32  deg2rad            = 3.1415926535 / 180.;


static inline l_int32 min (l_int32 a, l_int32 b) {
    return b + ((a-b) & (a-b)>>31);
}

static inline l_int32 max (l_int32 a, l_int32 b) {
    return a - ((a-b) & (a-b)>>31);
}

//FIXME: left limit for angle=0 should be zero, returns 1
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

/// CalculateSADcol()
/// calculate sum of absolute differences of two rows of adjacent columns
/// last SAD calculation is for row i=right and i=right+1.
///____________________________________________________________________________
l_uint32 CalculateSADcol(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_float32  hPercent,
                         l_int32    *reti,
                         l_uint32   *retDiff
                        )
{

    l_uint32 i, j;
    l_uint32 acc=0;
    l_uint32 a,b;
    l_uint32 maxDiff=0;
    l_int32 maxi=-1;
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(left>=0);
    assert(left<right);
    assert(right<w);

    //kernel has height of (h/2 +/- h*hPercent/2)
    l_uint32 jTop = (l_uint32)((1-hPercent)*0.5*h);
    l_uint32 jBot = (l_uint32)((1+hPercent)*0.5*h);
    //printf("jTop/Bot is %d/%d\n", jTop, jBot);

    for (i=left; i<right; i++) {
        //printf("%d: ", i);
        acc=0;
        for (j=jTop; j<jBot; j++) {
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

/// CalculateSADcol()
/// calculate sum of absolute differences of two rows of adjacent columns
/// last SAD calculation is for row i=right and i=right+1.
///____________________________________________________________________________
l_uint32 CalculateSADrow(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   top,
                         l_uint32   bottom,
                         l_int32    *reti,
                         l_uint32   *retDiff
                        )
{

    l_uint32 i, j;
    l_uint32 acc=0;
    l_uint32 a,b;
    l_uint32 maxDiff=0;
    l_int32 maxj=-1;
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(left>=0);
    assert(left<right);
    assert(right<w);
    assert(top>=0);
    assert(top<bottom);
    assert(bottom<h);


    for (j=top; j<bottom; j++) {
        //printf("%d: ", i);
        acc=0;
        for (i=left; i<right; i++) {
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

    *reti = maxj;
    *retDiff = maxDiff;
    return (-1 != maxj);
}

/// CalculateAvgCol()
/// calculate avg luma of a column
/// last SAD calculation is for row i=right and i=right+1.
///____________________________________________________________________________
double CalculateAvgCol(PIX        *pixg,
                         l_uint32   i,
                         l_float32  hPercent
                        )
{

    l_uint32 acc=0;
    l_uint32 a, j;
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(i>=0);
    assert(i<w);

    //kernel has height of (h/2 +/- h*hPercent/2)
    l_uint32 jTop = (l_uint32)((1-hPercent)*0.5*h);
    l_uint32 jBot = (l_uint32)((1+hPercent)*0.5*h);
    //printf("jTop/Bot is %d/%d\n", jTop, jBot);

    acc=0;
    for (j=jTop; j<jBot; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        acc += a;
    }
    //printf("%d \n", acc);        

    double avg = acc;
    avg /= (jBot-jTop);
    return avg;
}


/// FindGutterCrop()
/// This funciton finds the gutter-side (binding-side) crop line.
/// TODO: The return value should indicate a confidence.
///____________________________________________________________________________
l_uint32 FindGutterCrop(PIX *pixg, l_int32 rotDir) {

    //Currently, we can only do right-hand leafs
    assert(1 == rotDir);

    #define kKernelHeight 0.30

    //Assume we can find the binding within the first 10% of the image width
    l_uint32 width   = pixGetWidth(pixg);
    l_uint32 width10 = (l_uint32)(width * 0.10);

    l_int32    strongEdge;
    l_uint32   strongEdgeDiff;
    //TODO: calculate left bound based on amount of BRING_IN_BLACK due to rotation
    CalculateSADcol(pixg, 5, width10, kKernelHeight, &strongEdge, &strongEdgeDiff);
    printf("strongest edge of gutter is at i=%d with diff=%d\n", strongEdge, strongEdgeDiff);

    //TODO: what if strongEdge = 0 or something obviously bad?

    //Look for a second strong edge for the other side of the binding.
    //This edge should exist within +/- 3% of the image width.

    l_int32     secondEdgeL, secondEdgeR;
    l_uint32    secondEdgeDiffL, secondEdgeDiffR;
    l_uint32 width3p = (l_uint32)(width * 0.03);

    if (0 != strongEdge) {
        l_int32 searchLimit = max(0, strongEdge-width3p);

        CalculateSADcol(pixg, searchLimit, strongEdge-1, kKernelHeight, &secondEdgeL, &secondEdgeDiffL);
        printf("secondEdgeL = %d, diff = %d\n", secondEdgeL, secondEdgeDiffL);
    } else {
        //FIXME what to do here?
        return 0;
    }

    if (strongEdge < (width-2)) {
        l_int32 searchLimit = strongEdge + width3p;
        assert(searchLimit>strongEdge+1);
        CalculateSADcol(pixg, strongEdge+1, searchLimit, kKernelHeight, &secondEdgeR, &secondEdgeDiffR);
        printf("secondEdgeR = %d, diff = %d\n", secondEdgeR, secondEdgeDiffR);

    } else {
        //FIXME what to do here?
        return 0;
    }

    l_int32  secondEdge;
    l_uint32 secondEdgeDiff;
    
    if (secondEdgeDiffR > secondEdgeDiffL) {
        secondEdge = secondEdgeR;
        secondEdgeDiff = secondEdgeDiffR;
    } else if (secondEdgeDiffR < secondEdgeDiffL) {
        secondEdge = secondEdgeL;
        secondEdgeDiff = secondEdgeDiffL;
    } else {
        //FIXME
        return 0;
    }

    if ((secondEdgeDiff > (strongEdgeDiff*0.80)) && (secondEdgeDiff < (strongEdgeDiff*1.20))) {
        printf("Found gutter at %d!\n", strongEdge);
        return 1;
    }

    debugstr("Could not find gutter!\n");
    return 0;
}

/// FindBindingEdge()
///____________________________________________________________________________
l_uint32 FindBindingEdge(PIX     *pixg,
                         l_int32 rotDir,
                         float   *skew)
{

    //Currently, we can only do right-hand leafs
    assert(1 == rotDir);

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width10 = (l_uint32)(w * 0.10);
    

    // Find the strong edge, which should be one of the two sides of the binding
    // Rotate the image to maximize SAD

    l_int32    bindingEdge = -1;
    l_uint32   bindingEdgeDiff = 0;
    float      bindingDelta;
    float delta;
    for (delta=-1.0; delta<=1.0; delta+=0.05) {    
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);
        l_int32    strongEdge;
        l_uint32   strongEdgeDiff;
        l_uint32   limitLeft = calcLimitLeft(w,h,delta);
        //printf("limitLeft = %d\n", limitLeft);
        
        CalculateSADcol(pixt, limitLeft, width10, kKernelHeight, &strongEdge, &strongEdgeDiff);
        //printf("delta=%f, strongest edge of gutter is at i=%d with diff=%d\n", delta, strongEdge, strongEdgeDiff);
        if (strongEdgeDiff > bindingEdgeDiff) {
            bindingEdge = strongEdge;
            bindingEdgeDiff = strongEdgeDiff;
            bindingDelta = delta;
        }
        pixDestroy(&pixt);    
    }
    
    assert(-1 != bindingEdge); //TODO: handle error
    printf("BEST: delta=%f, strongest edge of gutter is at i=%d with diff=%d\n", bindingDelta, bindingEdge, bindingEdgeDiff);
    *skew = bindingDelta;

    // Now compute threshold for psudo-bitonalization
    // Use midpoint between avg luma of dark and light lines of binding edge

    PIX *pixt = pixRotate(pixg,
                    deg2rad*bindingDelta,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    pixWrite("/home/rkumar/public_html/outgray.jpg", pixt, IFF_JFIF_JPEG);     
    
    double bindingLumaA = CalculateAvgCol(pixt, bindingEdge, kKernelHeight);
    printf("lumaA = %f\n", bindingLumaA);

    double bindingLumaB = CalculateAvgCol(pixt, bindingEdge+1, kKernelHeight);
    printf("lumaB = %f\n", bindingLumaB);


    double threshold = (l_uint32)((bindingLumaA + bindingLumaB) / 2);
    //TODO: ensure this threshold is reasonable
    printf("thesh = %f\n", threshold);
    

    l_uint32 width3p = (l_uint32)(w * 0.03);
    l_uint32 rightEdge;
    l_uint32 numBlackLines = 0;
    
    if (bindingLumaA > bindingLumaB) { //found left edge
        l_uint32 i;
        l_uint32 rightLimit = bindingEdge+width3p;
        for (i=bindingEdge+1; i<rightLimit; i++) {
            double lumaAvg = CalculateAvgCol(pixt, i, kKernelHeight);
            printf("i=%d, avg=%f\n", i, lumaAvg);
            if (lumaAvg<threshold) {
                numBlackLines++;
            } else {
                rightEdge = i-1;
                break;
            }
        }
        printf("numBlackLines = %d\n", numBlackLines);
    
    } else if (bindingLumaA < bindingLumaB) { //found right edge
        l_uint32 i;
        l_uint32 leftLimit = bindingEdge-width3p;
        rightEdge = bindingEdge;
        if (leftLimit<0) leftLimit = 0;
        for (i=bindingEdge-1; i>leftLimit; i--) {
            double lumaAvg = CalculateAvgCol(pixt, i, kKernelHeight);
            printf("i=%d, avg=%f\n", i, lumaAvg);
            if (lumaAvg<threshold) {
                numBlackLines++;
            } else {
                break;
            }
        }
        printf("numBlackLines = %d\n", numBlackLines);
    
    } else {
        return -1; //TODO: handle error
    }
    
    if ((numBlackLines >=1) && (numBlackLines<width3p)) {
        return rightEdge;
    } else {
        return -1;
    }    
    
    return 1; //TODO: return error code on failure
}

/// FindOuterEdge()
///____________________________________________________________________________
l_uint32 FindOuterEdge(PIX     *pixg,
                       l_int32 rotDir,
                       float   *skew)
{

    //Currently, we can only do right-hand leafs
    assert(1 == rotDir);

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width75 = (l_uint32)(w * 0.75);
    

    l_int32    outerEdge = -1;
    l_uint32   outerEdgeDiff = 0;
    float      outerDelta;
    float      delta;
    for (delta=-1.0; delta<=1.0; delta+=0.05) {    
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);
        l_int32    strongEdge;
        l_uint32   strongEdgeDiff;
        l_uint32   limitLeft = calcLimitLeft(w,h,delta);
        //printf("limitLeft = %d\n", limitLeft);
        
        CalculateSADcol(pixt, width75, w-limitLeft-1, kKernelHeight, &strongEdge, &strongEdgeDiff); //TODO: is w-leftLimit-1 right?
        //printf("delta=%f, strongest outer edge at i=%d with diff=%d\n", delta, strongEdge, strongEdgeDiff);
        if (strongEdgeDiff > outerEdgeDiff) {
            outerEdge     = strongEdge;
            outerEdgeDiff = strongEdgeDiff;
            outerDelta    = delta;
        }
        pixDestroy(&pixt);    
    }
    
    assert(-1 != outerEdge); //TODO: handle error
    printf("BEST: delta=%f, outer edge is at i=%d with diff=%d\n", outerDelta, outerEdge, outerEdgeDiff);
    *skew = outerDelta;
}

/// FindHorizontalEdge()
///____________________________________________________________________________
l_uint32 FindHorizontalEdge(PIX      *pixg,
                     l_int32  rotDir,
                     l_uint32 bindingEdge,
                     bool     whichEdge,
                     float    *skew)
{
    //Although we assume the page is centered vertically, we can't assume that
    //the page is centered horizontally. 

    //Currently, we can only do right-hand leafs
    assert(1 == rotDir);

    //start at bindingEdge, and go 25% into the image.
    //TODO: generalize this to support both left and right hand leafs
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width50  = (l_uint32)(w * 0.5);
    l_uint32 height25 = (l_uint32)(h * 0.25);

    l_int32    strongEdge;
    l_uint32   strongEdgeDiff;

    l_int32    topEdge = -1;        //TODO: generalize - this should be horizEdge
    l_uint32   topEdgeDiff = 0;
    float      topDelta;
    float delta;
    for (delta=-1.0; delta<=1.0; delta+=0.05) {    
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);
        l_int32    strongEdge;
        l_uint32   strongEdgeDiff;
        l_uint32   topLimit = calcLimitTop(w,h,delta);
        

        l_uint32   top, bottom;
        if (0 == whichEdge) { //top Edge
            top = topLimit;
            bottom = height25;
        } else {
            bottom = h-topLimit-1; //TODO: is the -1 right?
            top    = h-height25;
        }

        CalculateSADrow(pixt, bindingEdge, bindingEdge+width50, top, bottom, &strongEdge, &strongEdgeDiff);
        //printf("delta=%f, strongest top edge is at i=%d with diff=%d\n", delta, strongEdge, strongEdgeDiff);
        if (strongEdgeDiff > topEdgeDiff) {
            topEdge = strongEdge;
            topEdgeDiff = strongEdgeDiff;
            topDelta = delta;
        }
        pixDestroy(&pixt);    
    }

    assert(-1 != topEdge); //TODO: handle error
    printf("BEST Horiz: delta=%f at j=%d with diff=%d\n", topDelta, topEdge, topEdgeDiff);
    *skew = topDelta;
    return topEdge;
}

/// main()
///____________________________________________________________________________
int main(int argc, char **argv) {
    PIX         *pixs, *pixd, *pixg;
    char        *filein;
    static char  mainName[] = "autoCropScribe";
    l_int32      rotDir;
    FILE        *fp;

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

    pixg = pixConvertRGBToGray (pixd, 0.30, 0.60, 0.10);
    debugstr("Converted to gray\n");
    //pixWrite("/home/rkumar/public_html/outgray.jpg", pixg, IFF_JFIF_JPEG); 

    float delta;

    #if 0
    for (delta=-1.0; delta<=1.0; delta+=0.05) {
        printf("delta = %f\n", delta);
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);

        FindGutterCrop(pixt, rotDir);
        pixDestroy(&pixt);
    }
    #endif
    //FindGutterCrop(pixg, rotDir);
    
    l_int32 cropT=-1, cropB=-1, cropR=-1, cropL=-1;
    float deltaT, deltaB, deltaV1, deltaV2;

    /// find binding side edge
    l_int32 bindingEdge = FindBindingEdge(pixg, rotDir, &deltaV1);
    
    if (-1 == bindingEdge) {
        printf("COULD NOT FIND BINDING!");
    } else {
        printf("FOUND binding edge= %d\n", bindingEdge);
    }

    /// find top edge
    l_int32 topEdge = FindHorizontalEdge(pixg, rotDir, bindingEdge, 0, &deltaT);

    /// find bottom edge
    l_int32 bottomEdge = FindHorizontalEdge(pixg, rotDir, bindingEdge, 1, &deltaB);

    /// find the outer vertical edge
    l_int32 outerEdge = FindOuterEdge(pixg, rotDir, &deltaV2);

    /// cleanup
    pixDestroy(&pixg);
    pixDestroy(&pixs);
    pixDestroy(&pixd);
}
