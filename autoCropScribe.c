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
#include <float.h>  //for DBL_MAX

#define debugstr printf
//#define debugstr


static const l_float32  deg2rad            = 3.1415926535 / 180.;


static inline l_int32 min (l_int32 a, l_int32 b) {
    return b + ((a-b) & (a-b)>>31);
}

static inline l_int32 max (l_int32 a, l_int32 b) {
    return a - ((a-b) & (a-b)>>31);
}


#define DEBUGMOV 0

#if DEBUGMOV
#include <libgen.h> //for basename()
struct DebugMov{
    double angle;
    char   *outDir;
    char   *filename;
    int    framenum;
    int    edgeBinding;
    int    edgeOuter;
} debugmov;
#endif //DEBUGMOV

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

/// CalculateAvgCol()
/// calculate avg luma of a column
/// last SAD calculation is for row i=right and i=right+1.
///____________________________________________________________________________
double CalculateAvgCol(PIX      *pixg,
                       l_uint32 i,
                       l_uint32 jTop,
                       l_uint32 jBot)
{

    l_uint32 acc=0;
    l_uint32 a, j;
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(i>=0);
    assert(i<w);

    //kernel has height of (h/2 +/- h*hPercent/2)
    //l_uint32 jTop = (l_uint32)((1-hPercent)*0.5*h);
    //l_uint32 jBot = (l_uint32)((1+hPercent)*0.5*h);
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

/// CalculateAvgRow()
/// calculate avg luma of a row
///____________________________________________________________________________
double CalculateAvgRow(PIX      *pixg,
                       l_uint32 j,
                       l_uint32 iLeft,
                       l_uint32 iRight)
{

    l_uint32 acc=0;
    l_uint32 a, i;
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(j>=0);
    assert(j<h);


    acc=0;
    for (i=iLeft; i<iRight; i++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        acc += a;
    }
    //printf("%d \n", acc);        

    double avg = acc;
    avg /= (iRight-iLeft);
    return avg;
}

/// CalculateAvgBlock()
/// calculate avg luma of a block
///____________________________________________________________________________
double CalculateAvgBlock(PIX      *pixg,
                       l_uint32 left,
                       l_uint32 right,
                       l_uint32 top,
                       l_uint32 bottom)
{

    l_uint32 acc=0;
    l_uint32 a, i, j;
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(top>=0);
    assert(left>=0);
    assert(bottom<h);
    assert(right<w);

    acc=0;
    for (i=left; i<=right; i++) {
        for (j=top; j<=bottom; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            acc += a;
        }
    }
    //printf("%d \n", acc);        

    double avg = acc;
    avg /= ((right-left+1)*(bottom-top+1));
    return avg;
}

/// CalculateSADcol()
/// calculate sum of absolute differences of two rows of adjacent columns
/// last SAD calculation is for row i=right and i=right+1.
///____________________________________________________________________________
l_uint32 CalculateSADcol(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   jTop,
                         l_uint32   jBot,
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
    //l_uint32 jTop = (l_uint32)((1-hPercent)*0.5*h);
    //l_uint32 jBot = (l_uint32)((1+hPercent)*0.5*h);
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
        
        #if DEBUGMOV
        {
            debugmov.framenum++;
            char cmd[512];
            int ret = snprintf(cmd, 512, "convert /home/rkumar/public_html/debugmov/smallgray.jpg -background black -rotate %f -pointsize 18 -fill yellow -annotate 0x0+10+20 '%s' -fill red -annotate 0x0+10+40 'angle = %0.2f' -draw 'line %d,%d %d,%d' -fill green -draw 'line %d,%d %d,%d' -draw 'line %d,%d %d,%d' \"%s/frames/%06d.jpg\"", debugmov.angle, debugmov.filename, debugmov.angle, i, jTop, i, jBot, debugmov.edgeBinding, jTop, debugmov.edgeBinding, jBot, debugmov.edgeOuter, jTop, debugmov.edgeOuter, jBot, debugmov.outDir, debugmov.framenum);
            assert(ret);
            printf(cmd);
            printf("\n");
            ret = system(cmd);
            assert(0 == ret);
        }
        #endif //DEBUGMOV
    }

    *reti = maxi;
    *retDiff = maxDiff;
    return (-1 != maxi);
}


/// CalculateSADrow()
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

/// FindBestVarRow()
/// find row with least variance
///____________________________________________________________________________
l_uint32 FindMinVarRow(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   top,
                         l_uint32   bottom,
                         double     thresh,
                         l_int32    *retj,
                         double     *retVar
                        )
{

    l_uint32 i, j;
    l_uint32 a;
    double minVar=DBL_MAX;
    l_int32 minj=-1;
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(left>=0);
    assert(left<right);
    assert(right<w);
    assert(top>=0);
    assert(top<bottom);
    assert(bottom<h);
    double var;
    l_uint32 width20 = (l_uint32)(w * 0.20);

    for (j=top; j<=bottom; j++) {
        //printf("%d: ", j);
        var = 0;
        double avg = CalculateAvgRow(pixg, j, left+width20, right-width20);
        if (avg<thresh) {
            //printf("avg too low, continuing! (%f)\n", avg);
            continue;
        }
        for (i=left+width20; i<right-width20; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            double diff = avg-a;
            var += (diff * diff);
        }
        //printf("var=%f avg=%f\n", var, avg);
        if (var < minVar) {
            minVar = var;
            minj   = j; 
        }
        
    }

    *retj = minj;
    *retVar = minVar;
    return (-1 != minj);
}

/// FindBestVarCol()
/// find col with least variance
///____________________________________________________________________________
l_uint32 FindMinVarCol(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   top,
                         l_uint32   bottom,
                         double     thresh,
                         l_int32    *reti,
                         double     *retVar
                        )
{

    l_uint32 i, j;
    l_uint32 a;
    double minVar=DBL_MAX;
    l_int32 mini=-1;
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(left>=0);
    assert(left<right);
    assert(right<w);
    assert(top>=0);
    assert(top<bottom);
    assert(bottom<h);
    double var;
    l_uint32 h20 = (l_uint32)(h * 0.20);

    for (i=left; i<=right; i++) {
        //printf("%d: ", i);
        var = 0;
        double avg = CalculateAvgCol(pixg, i, top+h20, bottom-h20);
        if (avg<thresh) {
            //printf("avg too low, continuing! (%f)\n", avg);
            continue;
        }
        for (j=top+h20; j<bottom-h20; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            double diff = avg-a;
            var += (diff * diff);
        }
        //printf("var=%f avg=%f\n", var, avg);
        if (var < minVar) {
            minVar = var;
            mini   = i; 
        }
        
    }

    *reti = mini;
    *retVar = minVar;
    return (-1 != mini);
}

/// CalculateFullPageSADrow()
/// calculate sum of absolute differences of two rows of adjacent columns
/// last SAD calculation is for row i=right and i=right+1.
///____________________________________________________________________________
double CalculateFullPageSADrow(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   top,
                         l_uint32   bottom
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
        for (i=left; i<right; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i, j+1, &b);
            assert(0 == retval);
            //printf("%d ", val);
            acc += (abs(a-b));
        }
 
    }

    double sum = (double)acc;
    //printf("acc=%d, sum=%f\n", acc, sum);
    return sum;
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

    l_uint32 h = pixGetHeight( pixg );
    //kernel has height of (h/2 +/- h*hPercent/2)
    l_uint32 jTop = (l_uint32)((1-kKernelHeight)*0.5*h);
    l_uint32 jBot = (l_uint32)((1+kKernelHeight)*0.5*h);

    l_int32    strongEdge;
    l_uint32   strongEdgeDiff;
    //TODO: calculate left bound based on amount of BRING_IN_BLACK due to rotation
    CalculateSADcol(pixg, 5, width10, jTop, jBot, &strongEdge, &strongEdgeDiff);
    printf("strongest edge of gutter is at i=%d with diff=%d\n", strongEdge, strongEdgeDiff);

    //TODO: what if strongEdge = 0 or something obviously bad?

    //Look for a second strong edge for the other side of the binding.
    //This edge should exist within +/- 3% of the image width.

    l_int32     secondEdgeL, secondEdgeR;
    l_uint32    secondEdgeDiffL, secondEdgeDiffR;
    l_uint32 width3p = (l_uint32)(width * 0.03);

    if (0 != strongEdge) {
        l_int32 searchLimit = max(0, strongEdge-width3p);

        CalculateSADcol(pixg, searchLimit, strongEdge-1, jTop, jBot, &secondEdgeL, &secondEdgeDiffL);
        printf("secondEdgeL = %d, diff = %d\n", secondEdgeL, secondEdgeDiffL);
    } else {
        //FIXME what to do here?
        return 0;
    }

    if (strongEdge < (width-2)) {
        l_int32 searchLimit = strongEdge + width3p;
        assert(searchLimit>strongEdge+1);
        CalculateSADcol(pixg, strongEdge+1, searchLimit, jTop, jBot, &secondEdgeR, &secondEdgeDiffR);
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
l_uint32 FindBindingEdge(PIX      *pixg,
                         l_int32  rotDir,
                         float    *skew,
                         l_uint32 *thesh)
{

    //Currently, we can only do right-hand leafs
    assert((1 == rotDir) || (-1 == rotDir));

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width10 = (l_uint32)(w * 0.10);

    //kernel has height of (h/2 +/- h*hPercent/2)
    l_uint32 jTop = (l_uint32)((1-kKernelHeight)*0.5*h);
    l_uint32 jBot = (l_uint32)((1+kKernelHeight)*0.5*h);    

    // Find the strong edge, which should be one of the two sides of the binding
    // Rotate the image to maximize SAD

    l_int32    bindingEdge = -1;
    l_uint32   bindingEdgeDiff = 0;
    float      bindingDelta;
    float delta;
    //0.05 degrees is a good increment for the final search
    for (delta=-1.0; delta<=1.0; delta+=0.2) {    
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);
        l_int32    strongEdge;
        l_uint32   strongEdgeDiff;
        l_uint32   limitLeft = calcLimitLeft(w,h,delta);
        //printf("limitLeft = %d\n", limitLeft);

        #if DEBUGMOV
        debugmov.angle = delta;
        #endif //DEBUGMOV

        l_uint32 left, right;
        if (1 == rotDir) {
            left  = limitLeft;
            right = width10;
        } else {
            left  = w - width10;
            right = w - limitLeft-1;
        }

        CalculateSADcol(pixt, left, right, jTop, jBot, &strongEdge, &strongEdgeDiff);
        //printf("delta=%f, strongest edge of gutter is at i=%d with diff=%d, w,h=(%d,%d)\n", delta, strongEdge, strongEdgeDiff, w, h);
        if (strongEdgeDiff > bindingEdgeDiff) {
            bindingEdge = strongEdge;
            bindingEdgeDiff = strongEdgeDiff;
            bindingDelta = delta;

            #if DEBUGMOV
            debugmov.edgeBinding = bindingEdge;
            #endif //DEBUGMOV
        }
        

        pixDestroy(&pixt);    
    }
    
    assert(-1 != bindingEdge); //TODO: handle error
    printf("BEST: delta=%f, strongest edge of gutter is at i=%d with diff=%d\n", bindingDelta, bindingEdge, bindingEdgeDiff);
    *skew = bindingDelta;
    #if DEBUGMOV
    debugmov.angle = bindingDelta;
    #endif //DEBUGMOV

    // Now compute threshold for psudo-bitonalization
    // Use midpoint between avg luma of dark and light lines of binding edge

    PIX *pixt = pixRotate(pixg,
                    deg2rad*bindingDelta,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    //pixWrite("/home/rkumar/public_html/outgray.jpg", pixt, IFF_JFIF_JPEG);
    
    double bindingLumaA = CalculateAvgCol(pixt, bindingEdge, jTop, jBot);
    printf("lumaA = %f\n", bindingLumaA);

    double bindingLumaB = CalculateAvgCol(pixt, bindingEdge+1, jTop, jBot);
    printf("lumaB = %f\n", bindingLumaB);

    /*
    {
        int i;
        for (i=bindingEdge-10; i<bindingEdge+10; i++) {
            double bindingLuma = CalculateAvgCol(pixt, i, jTop, jBot);
            printf("i=%d, luma=%f\n", i, bindingLuma);
        }
    }
    */


    double threshold = (l_uint32)((bindingLumaA + bindingLumaB) / 2);
    //TODO: ensure this threshold is reasonable
    printf("thesh = %f\n", threshold);
    
    *thesh = (l_uint32)threshold;

    l_uint32 width3p = (l_uint32)(w * 0.03);
    l_uint32 rightEdge;
    l_uint32 numBlackLines = 0;
    
    if (bindingLumaA > bindingLumaB) { //found left edge
        l_uint32 i;
        l_uint32 rightLimit = bindingEdge+width3p;
        for (i=bindingEdge+1; i<rightLimit; i++) {
            double lumaAvg = CalculateAvgCol(pixt, i, jTop, jBot);
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
        printf("found right edge of gutter, leftLimit=%d, rightLimit=%d\n", leftLimit, bindingEdge-1);
        for (i=bindingEdge-1; i>leftLimit; i--) {
            double lumaAvg = CalculateAvgCol(pixt, i, jTop, jBot);
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
    
    ///temp code to calculate some thesholds..
    /*
    l_uint32 a, j, i = rightEdge;
    l_uint32 numBlackPels = 0;
    for (j=jTop; j<jBot; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<threshold) {
            numBlackPels++;
        }
    }
    printf("%d: numBlack=%d\n", i, numBlackPels);
    i = rightEdge+1;
    numBlackPels = 0;
    for (j=jTop; j<jBot; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<threshold) {
            numBlackPels++;
        }
    }
    printf("%d: numBlack=%d\n", i, numBlackPels);
    */
    ///end temp code

    if ((numBlackLines >=1) && (numBlackLines<width3p)) {
        return rightEdge;
    } else {
        debugstr("COULD NOT FIND BINDING, using strongest edge!\n");
        return bindingEdge;
    }    
    
    return 1; //TODO: return error code on failure
}


/// FindBindingEdge2()
///____________________________________________________________________________
l_uint32 FindBindingEdge2(PIX      *pixg,
                         l_int32  rotDir,
                         l_uint32 topEdge,
                         l_uint32 bottomEdge,
                         float    *skew,
                         l_uint32 *thesh)
{

    //Currently, we can only do right-hand leafs
    assert((1 == rotDir) || (-1 == rotDir));

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width10 = (l_uint32)(w * 0.10);

    //kernel has height of (h/2 +/- h*hPercent/2)
    l_uint32 kernelHeight10 = (l_uint32)(0.10*(bottomEdge-topEdge));
    //l_uint32 jTop = (l_uint32)((1-kKernelHeight)*0.5*h);
    //l_uint32 jBot = (l_uint32)((1+kKernelHeight)*0.5*h);    
    l_uint32 jTop = topEdge+kernelHeight10;
    l_uint32 jBot = bottomEdge-kernelHeight10;

    // Find the strong edge, which should be one of the two sides of the binding
    // Rotate the image to maximize SAD

    l_int32    bindingEdge = -1;
    l_uint32   bindingEdgeDiff = 0;
    float      bindingDelta;
    float delta;
    //0.05 degrees is a good increment for the final search
    for (delta=-1.0; delta<=1.0; delta+=0.2) {    
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);
        l_int32    strongEdge;
        l_uint32   strongEdgeDiff;
        l_uint32   limitLeft = calcLimitLeft(w,h,delta);
        //printf("limitLeft = %d\n", limitLeft);

        #if DEBUGMOV
        debugmov.angle = delta;
        #endif //DEBUGMOV

        l_uint32 left, right;
        if (1 == rotDir) {
            left  = limitLeft;
            right = width10;
        } else {
            left  = w - width10;
            right = w - limitLeft-1;
        }

        CalculateSADcol(pixt, left, right, jTop, jBot, &strongEdge, &strongEdgeDiff);
        //printf("delta=%f, strongest edge of gutter is at i=%d with diff=%d, w,h=(%d,%d)\n", delta, strongEdge, strongEdgeDiff, w, h);
        if (strongEdgeDiff > bindingEdgeDiff) {
            bindingEdge = strongEdge;
            bindingEdgeDiff = strongEdgeDiff;
            bindingDelta = delta;

            #if DEBUGMOV
            debugmov.edgeBinding = bindingEdge;
            #endif //DEBUGMOV
        }
        

        pixDestroy(&pixt);    
    }
    
    assert(-1 != bindingEdge); //TODO: handle error
    printf("BEST: delta=%f, strongest edge of gutter is at i=%d with diff=%d\n", bindingDelta, bindingEdge, bindingEdgeDiff);
    *skew = bindingDelta;
    #if DEBUGMOV
    debugmov.angle = bindingDelta;
    #endif //DEBUGMOV

    // Now compute threshold for psudo-bitonalization
    // Use midpoint between avg luma of dark and light lines of binding edge

    PIX *pixt = pixRotate(pixg,
                    deg2rad*bindingDelta,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    //pixWrite("/home/rkumar/public_html/outgray.jpg", pixt, IFF_JFIF_JPEG);
    
    double bindingLumaA = CalculateAvgCol(pixt, bindingEdge, jTop, jBot);
    printf("lumaA = %f\n", bindingLumaA);

    double bindingLumaB = CalculateAvgCol(pixt, bindingEdge+1, jTop, jBot);
    printf("lumaB = %f\n", bindingLumaB);

    /*
    {
        int i;
        for (i=bindingEdge-10; i<bindingEdge+10; i++) {
            double bindingLuma = CalculateAvgCol(pixt, i, jTop, jBot);
            printf("i=%d, luma=%f\n", i, bindingLuma);
        }
    }
    */


    double threshold = (l_uint32)((bindingLumaA + bindingLumaB) / 2);
    //TODO: ensure this threshold is reasonable
    printf("thesh = %f\n", threshold);
    
    *thesh = (l_uint32)threshold;

    l_uint32 width3p = (l_uint32)(w * 0.03);
    l_uint32 rightEdge;
    l_uint32 numBlackLines = 0;
    
    if (bindingLumaA > bindingLumaB) { //found left edge
        l_uint32 i;
        l_uint32 rightLimit = bindingEdge+width3p;
        for (i=bindingEdge+1; i<rightLimit; i++) {
            double lumaAvg = CalculateAvgCol(pixt, i, jTop, jBot);
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
        printf("found right edge of gutter, leftLimit=%d, rightLimit=%d\n", leftLimit, bindingEdge-1);
        for (i=bindingEdge-1; i>leftLimit; i--) {
            double lumaAvg = CalculateAvgCol(pixt, i, jTop, jBot);
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
    
    ///temp code to calculate some thesholds..
    /*
    l_uint32 a, j, i = rightEdge;
    l_uint32 numBlackPels = 0;
    for (j=jTop; j<jBot; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<threshold) {
            numBlackPels++;
        }
    }
    printf("%d: numBlack=%d\n", i, numBlackPels);
    i = rightEdge+1;
    numBlackPels = 0;
    for (j=jTop; j<jBot; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<threshold) {
            numBlackPels++;
        }
    }
    printf("%d: numBlack=%d\n", i, numBlackPels);
    */
    ///end temp code

    if ((numBlackLines >=1) && (numBlackLines<width3p)) {
        return rightEdge;
    } else {
        debugstr("COULD NOT FIND BINDING, using strongest edge!\n");
        return bindingEdge;
    }    
    
    return 1; //TODO: return error code on failure
}

/// FindOuterEdge()
///____________________________________________________________________________
l_int32 FindOuterEdge(PIX     *pixg,
                       l_int32 rotDir,
                       float   *skew,
                       l_uint32 *threshOuter)
{

    //Currently, we can only do right-hand leafs
    assert((1 == rotDir) || (-1 == rotDir));

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width75 = (l_uint32)(w * 0.75);
    l_uint32 width25 = (l_uint32)(w * 0.25);
    
    //kernel has height of (h/2 +/- h*hPercent/2)
    l_uint32 jTop = (l_uint32)((1-kKernelHeight)*0.5*h);
    l_uint32 jBot = (l_uint32)((1+kKernelHeight)*0.5*h);
    


    l_int32    outerEdge = -1;
    l_uint32   outerEdgeDiff = 0;
    float      outerDelta;
    float      delta;
    //0.05 is a good increment, but too fine for testing
    for (delta=-1.0; delta<=1.0; delta+=0.2) {
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);
        l_int32    strongEdge;
        l_uint32   strongEdgeDiff;
        l_uint32   limitLeft = calcLimitLeft(w,h,delta);
        //printf("limitLeft = %d\n", limitLeft);

        #if DEBUGMOV
        debugmov.angle = delta;
        #endif //DEBUGMOV

        l_uint32 left, right;
        if (1 == rotDir) {
            left  = width75;
            right = w-limitLeft-1; //TODO: is w-leftLimit-1 right?
        } else if (-1 == rotDir) {
            left  = limitLeft;
            right = width25;
        } else {
            assert(0);
        }

        CalculateSADcol(pixt, left, right, jTop, jBot, &strongEdge, &strongEdgeDiff);
        //printf("delta=%f, strongest outer edge at i=%d with diff=%d\n", delta, strongEdge, strongEdgeDiff);
        if (strongEdgeDiff > outerEdgeDiff) {
            outerEdge     = strongEdge;
            outerEdgeDiff = strongEdgeDiff;
            outerDelta    = delta;

            #if DEBUGMOV
            debugmov.edgeOuter = outerEdge;
            #endif //DEBUGMOV
        }
        pixDestroy(&pixt);    
    }
    
    assert(-1 != outerEdge); //TODO: handle error
    printf("BEST: delta=%f, outer edge is at i=%d with diff=%d\n", outerDelta, outerEdge, outerEdgeDiff);
    
    
    //calculate threshold
    //l_uint32 jTop = (l_uint32)((1-kKernelHeight)*0.5*h);
    //l_uint32 jBot = (l_uint32)((1+kKernelHeight)*0.5*h);    

    PIX *pixt = pixRotate(pixg,
                    deg2rad*outerDelta,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    double bindingLumaA = CalculateAvgCol(pixt, outerEdge, jTop, jBot);
    printf("outer lumaA = %f\n", bindingLumaA);

    double bindingLumaB = CalculateAvgCol(pixt, outerEdge+1, jTop, jBot);
    printf("outer lumaB = %f\n", bindingLumaB);


    double threshold = (l_uint32)((bindingLumaA + bindingLumaB) / 2);
    //TODO: ensure this threshold is reasonable
    printf("outer thesh = %f\n", threshold);    
    *threshOuter = (l_uint32)threshold;
    pixDestroy(&pixt);    
    
    
    *skew = outerDelta;
    return outerEdge;
}

/// FindHorizontalEdge()
///____________________________________________________________________________
l_uint32 FindHorizontalEdge(PIX      *pixg,
                     l_int32  rotDir,
                     l_uint32 bindingEdge,
                     bool     whichEdge,
                     float    *skew,
                     l_uint32 *threshOut)
{
    //Although we assume the page is centered vertically, we can't assume that
    //the page is centered horizontally. 

    //Currently, we can only do right-hand leafs
    assert((1 == rotDir) || (-1 == rotDir));

    //start at bindingEdge, and go 25% into the image.
    //TODO: generalize this to support both left and right hand leafs
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width50  = (l_uint32)(w * 0.5);
    l_uint32 height25 = (l_uint32)(h * 0.25);


    l_uint32 left, right;
    if (1 == rotDir) {
        left  = bindingEdge;
        right = bindingEdge+width50;
    } else if (-1 == rotDir) {
        left  = bindingEdge-width50;
        right = bindingEdge;
    } else {
        assert(0);
    }

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



        CalculateSADrow(pixt, left, right, top, bottom, &strongEdge, &strongEdgeDiff);
        //printf("delta=%f, strongest top edge is at i=%d with diff=%d\n", delta, strongEdge, strongEdgeDiff);
        if (strongEdgeDiff > topEdgeDiff) {
            topEdge = strongEdge;
            topEdgeDiff = strongEdgeDiff;
            topDelta = delta;
        }
        pixDestroy(&pixt);    
    }

    
    //calculate threshold
    PIX *pixt = pixRotate(pixg,
                    deg2rad*topDelta,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
                    
    double bindingLumaA = CalculateAvgRow(pixt, topEdge, left, right);
    printf("horiz%d lumaA = %f\n", whichEdge, bindingLumaA);

    double bindingLumaB = CalculateAvgRow(pixt, topEdge+1, left, right);
    printf("horiz%d lumaB = %f\n", whichEdge, bindingLumaB);


    *threshOut = (l_uint32)((bindingLumaA + bindingLumaB) / 2);
    //TODO: ensure this threshold is reasonable
    printf("horiz%d thesh = %d\n", whichEdge, *threshOut);
    pixDestroy(&pixt);    

    assert(-1 != topEdge); //TODO: handle error
    printf("BEST Horiz: delta=%f at j=%d with diff=%d\n", topDelta, topEdge, topEdgeDiff);
    
    
    
    *skew = topDelta;
    return topEdge;
}

/// CalculateDifferentialSquareSum()
///____________________________________________________________________________
double CalculateDifferentialSquareSum(PIX *pixg, 
                                      l_uint32 cL,
                                      l_uint32 cR, 
                                      l_uint32 cT, 
                                      l_uint32 cB) 
{
    l_uint32 i, j;
    l_uint32 a, b;
    l_uint32 lineSum0, lineSum1;
    double sum=0;

    //init lineSum0;
    lineSum0=0;
    for (i=cL; i<=cR; i++) {
        l_int32 retval = pixGetPixel(pixg, i, cT, &a);
        assert(0 == retval);
        lineSum0 += a;
    }

    for (j=cT+1; j<cB; j++) {
        lineSum1 = 0;
        for (i=cL; i<=cR; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            lineSum1 +=a;
        }
        double diff = (double)lineSum0 - (double)lineSum1;
        sum += (diff*diff);
        //printf("\tl0=%d, l1=%d, diff=%f, sum=%f\n", lineSum0, lineSum1, diff, sum);
        lineSum0 = lineSum1;
    }

    return sum;
}

/// Deskew()
/// This works if you pass in a bitonal image, but doesn't work well with grayscale
///____________________________________________________________________________
int Deskew(PIX      *pixg, 
           l_int32 cropL, 
           l_int32 cropR, 
           l_int32 cropT, 
           l_int32 cropB, 
           double *skew, 
           double *skewConf)
{
    assert(cropR>cropL);
    assert(cropB>cropT);

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width10  = (l_uint32)(w * 0.10);
    l_uint32 height10 = (l_uint32)(h * 0.10);
    


    //first, reduce cropbox by 10% to get rid of non-page pixels
    printf("before reduce: cL=%d, cR=%d, cT=%d, cB=%d, w=%d, h=%d\n", cropL, cropR, cropT, cropB, w,h);
    if ( ((cropR-cropL) > (2*width10)) && ((cropB-cropT) > (2*height10)) ) {
        cropL += width10;
        cropR -= width10;
        cropT += height10;
        cropB -= height10;
    }
    printf("after reduce: cL=%d, cR=%d, cT=%d, cB=%d\n", cropL, cropR, cropT, cropB);

    double sumMax = CalculateDifferentialSquareSum(pixg, cropL, cropR, cropT, cropB);
    //double sumMax = CalculateFullPageSADrow(pixg, cropL, cropR, cropT, cropB);
    printf("init sumMax=%f\n", sumMax);
    double sumMin = sumMax;;
    float deltaMax = 0.0;

    float delta;
    for (delta=-1.0; delta<=1.0; delta+=0.05) {
        if ((-0.01<delta) && (delta<0.01)) continue;
        PIX *pixt = pixRotate(pixg,
                        deg2rad*delta,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);

        l_uint32   limitTop  = calcLimitTop(w,h,delta);
        l_uint32   limitLeft = calcLimitLeft(w,h,delta);

        l_uint32 cL = (cropL<limitLeft)     ? limitLeft     : cropL;
        l_uint32 cR = (cropR>(w-limitLeft)) ? (w-limitLeft) : cropR;
        l_uint32 cT = (cropT<limitTop)      ? limitTop      : cropT;
        l_uint32 cB = (cropB>(h-limitTop))  ? (h-limitTop)  : cropB;
        //printf("after trim: cL=%d, cR=%d, cT=%d, cB=%d\n", cL, cR, cT, cB);

        double sum = CalculateDifferentialSquareSum(pixt, cL, cR, cT, cB);
        //double sum = CalculateFullPageSADrow(pixt, cL, cR, cT, cB);
        if (sum > sumMax) {
            sumMax = sum;
            deltaMax = delta;
        }
        if (sum < sumMin) {
            sumMin = sum;
        }

        *skew = deltaMax;
        *skewConf = (sumMax/sumMin);
        printf("delta = %f, sum=%f\n", delta, sum);

    }
    printf("skew = %f, conf = %f\n", *skew, *skewConf);
    return 0;
}

/// AdjustCropBox()
///____________________________________________________________________________
int AdjustCropBox(PIX     *pixg,
                  l_int32 *cropL,
                  l_int32 *cropR,
                  l_int32 *cropT,
                  l_int32 *cropB,
                  l_int32 delta)
{
    l_int32 newL = *cropL;
    l_int32 newR = *cropR;
    l_int32 newT = *cropT;
    l_int32 newB = *cropB;
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_int32 limitLeft  = newL-delta;
    l_int32 limitRight = newL+delta;
    
    limitLeft  = max(0, limitLeft);
    limitRight = min(w-1, limitRight);
    
    l_int32  strongEdge;
    l_uint32 strongEdgeDiff;

    //printf("w,h = (%d,%d)  t,b = (%d,%d)\n", w, h, newT, newB);
    CalculateSADcol(pixg, limitLeft, limitRight, newT, newB, &strongEdge, &strongEdgeDiff);
    printf("AdjustCropBox Left i=%d with diff=%d\n", strongEdge, strongEdgeDiff);
    assert(-1 != strongEdge);
    newL = strongEdge;
    l_int32 vari;
    double var;
    FindMinVarCol(pixg, limitLeft, limitRight, newT, newB, 140, &vari, &var);
    printf("LEFT: min var found at i=%d, var=%f\n", vari, var);
    newL = vari;

    limitLeft  = newR-delta;
    limitRight = newR+delta;
    
    limitLeft  = max(0, limitLeft);
    limitRight = min(w-1, limitRight);

    CalculateSADcol(pixg, limitLeft, limitRight, newT, newB, &strongEdge, &strongEdgeDiff);
    printf("AdjustCropBox Right i=%d with diff=%d\n", strongEdge, strongEdgeDiff);
    assert(-1 != strongEdge);
    newR = strongEdge;
    FindMinVarCol(pixg, limitLeft, limitRight, newT, newB, 140, &vari, &var);
    printf("RIGHT: min var found at i=%d, var=%f\n", vari, var);
    newR = vari;


    l_int32 limitTop  = newT-delta;
    l_int32 limitBot  = newT+delta;
    
    limitTop  = max(0, limitTop);
    limitBot  = min(h-1, limitBot);

    //CalculateSADrow(pixg, newL, newR, limitTop, limitBot, &strongEdge, &strongEdgeDiff);
    //printf("AdjustCropBox Top j=%d with diff=%d\n", strongEdge, strongEdgeDiff);
    //assert(-1 != strongEdge);
    //newT = strongEdge;
    l_int32 varj; 
    FindMinVarRow(pixg, newL, newR, limitTop, limitBot, 140, &varj, &var);
    printf("TOP: min var found at j=%d, var=%f\n", varj, var);
    newT = varj;

    limitTop  = newB-delta;
    limitBot  = newB+delta;
    
    limitTop  = max(0, limitTop);
    limitBot  = min(h-1, limitBot);

    //CalculateSADrow(pixg, newL, newR, limitTop, limitBot, &strongEdge, &strongEdgeDiff);
    //printf("AdjustCropBox Bot j=%d with diff=%d\n", strongEdge, strongEdgeDiff);
    //assert(-1 != strongEdge);
    //newB = strongEdge;
    FindMinVarRow(pixg, newL, newR, limitTop, limitBot, 140, &varj, &var);
    printf("BOT: min var found at j=%d, var=%f\n", varj, var);
    newB = varj;

    *cropL = newL;
    *cropR = newR;
    *cropT = newT;
    *cropB = newB;

}

l_int32 FindMinBlockVarCol(PIX     *pixg,
                           l_int32 left,
                           l_int32 right,
                           l_int32 top,
                           l_int32 bottom, 
                           l_int32 kernelWidth,
                           l_int32 *reti,
                           double  *retVar)
{
    assert( right>=(left+kernelWidth) );

        l_uint32 i, j, iCol;
    l_uint32 a;
    double minVar=DBL_MAX;
    l_int32 mini=-1;
    
    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    assert(left>=0);
    assert(left<right);
    assert(right<w);
    assert(top>=0);
    assert(top<bottom);
    assert(bottom<h);
    double var;
    l_uint32 h20 = (l_uint32)(h * 0.20);

    l_int32 limitR = right-kernelWidth;
    printf("left=%d, right=%d, limitR=%d\n", left, right, limitR);

    double blockSize = kernelWidth*(bottom-top+1);

    for (iCol=left; iCol<=limitR; iCol++) {
        printf("%d: ", i);
        var = 0;
        double avg = CalculateAvgBlock(pixg, iCol, iCol+kernelWidth, top, bottom);
        for (j=top; j<=bottom; j++) {
            for(i=iCol; i<=(iCol+kernelWidth-1); i++) {
                l_int32 retval = pixGetPixel(pixg, i, j, &a);
                assert(0 == retval);
                double diff = avg-a;
                var += (diff * diff);
            }
        }
        var /= blockSize;
        printf("var=%f avg=%f\n", var, avg);
        if (var < minVar) {
            minVar = var;
            mini   = i; 
        }
        
    }

    *reti = mini;
    *retVar = minVar;
    return (-1 != mini);
}

/// AdjustCropBoxByVariance()
///____________________________________________________________________________
int AdjustCropBoxByVariance(PIX     *pixg,
                  l_int32 *cropL,
                  l_int32 *cropR,
                  l_int32 *cropT,
                  l_int32 *cropB,
                  l_int32 kernelWidth,
                  double  angle)
{
    l_int32 newL = *cropL;
    l_int32 newR = *cropR;
    l_int32 newT = *cropT;
    l_int32 newB = *cropB;

    l_uint32 w = pixGetWidth(pixg);
    l_uint32 h = pixGetHeight(pixg);
    l_int32 w10 = (l_int32)(w*0.10);

    l_int32  limitL = calcLimitLeft(w,h,angle);
    l_int32  left   = max(limitL, newL - 5);
    l_int32  right  = max(left+3, newL+w10);
 
   l_int32 varL;
    double var;
    FindMinBlockVarCol(pixg, left, right, newT, newB, 10, &varL, &var); 
    printf("VARBLOCKLEFT: %d\n", varL);
    newL = varL;

    left  = (l_int32)(0.75*w);
    right = (l_int32)(w-limitL);

    FindMinBlockVarCol(pixg, left, right, newT, newB, 10, &varL, &var); 
    printf("VARBLOCKRIGHT: %d\n", varL);
    newR = varL;

    *cropL = newL;
    *cropR = newR;
}

/// removeBlackPelsColRight()
///____________________________________________________________________________

l_uint32 removeBlackPelsColRight(PIX *pixg, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom) {
    l_uint32 i, j;
    l_uint32 a;

    l_uint32 numBlackPels=0;
    l_uint32 blackThresh=157;

    l_uint32 kernelHeight05 = (l_uint32)((bottom-top)*0.05);
    top += kernelHeight05;
    bottom -= kernelHeight05;

    numBlackPels = 0;
    for (j=top; j<bottom; j++) {
        l_int32 retval = pixGetPixel(pixg, starti-1, j, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }
    printf("init: numBlack=%d\n", numBlackPels);
    l_int32 allowedNumberOfBlackPels = (l_int32)(0.05 * numBlackPels);

    if (numBlackPels > 10) {
        printf(" needs adjustment!\n");
        for (i=starti-2; i>=endi; i--) {
            numBlackPels = 0;
            for (j=top; j<bottom; j++) {
                l_int32 retval = pixGetPixel(pixg, i, j, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
            printf("%d: numBlack=%d\n", i, numBlackPels);
            if (numBlackPels<5) {
                printf("break!\n");
                return i;
            }
        }
    }
    
    return starti;
    
}

/// RemoveBlackPelsBlockColRight()
///____________________________________________________________________________

l_uint32 RemoveBlackPelsBlockColRight(PIX *pixg, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom, l_uint32 kernelWidth, l_uint32 blackThresh) {
    l_uint32 i;
    l_uint32 a;

    l_uint32 numBlackPels=0;

    numBlackPels = 0;
    l_uint32 x, y;

    l_uint32 kernelHeight05 = (l_uint32)((bottom-top)*0.05);
    top += kernelHeight05;
    bottom -= kernelHeight05;

    printf("RIGHT: starti = %d, endi=%d, thresh=%d\n", starti, endi, blackThresh);

    for (i=starti-kernelWidth; i>=endi; i--) {
        numBlackPels = 0;
        for(x=i; x<i+kernelWidth; x++) {
            for(y=top; y<=bottom; y++) {
                l_int32 retval = pixGetPixel(pixg, x, y, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
        }
        //printf("R %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<5) {
            //printf("break!\n");
            return i;
        }

    }

    return starti;
    
}

/// RemoveBlackPelsBlockColLeft()
///____________________________________________________________________________

l_uint32 RemoveBlackPelsBlockColLeft(PIX *pixg, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom, l_uint32 kernelWidth, l_uint32 blackThresh) {
    l_uint32 i;
    l_uint32 a;

    l_uint32 numBlackPels=0;

    numBlackPels = 0;
    l_uint32 x, y;

    l_uint32 kernelHeight05 = (l_uint32)((bottom-top)*0.05);
    top += kernelHeight05;
    bottom -= kernelHeight05;

    //printf("LEFT: starti = %d, endi=%d, thresh=%d\n", starti, endi, blackThresh);

    for (i=starti+1; i<=endi; i++) {
        numBlackPels = 0;
        for(x=i; x<i+kernelWidth; x++) {
            for(y=top; y<=bottom; y++) {
                l_int32 retval = pixGetPixel(pixg, x, y, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
        }
        //printf("L %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<5) {
            //printf("break!\n");
            return i;
        }

    }

    return starti;
    
}

/// RemoveBlackPelsBlockRowTop()
///____________________________________________________________________________

l_uint32 RemoveBlackPelsBlockRowTop(PIX *pixg, l_uint32 startj, l_uint32 endj, l_uint32 left, l_uint32 right, l_uint32 kernelWidth, l_uint32 blackThresh) {
    l_uint32 j;
    l_uint32 a;

    l_uint32 numBlackPels=0;

    numBlackPels = 0;
    l_uint32 x, y;

    for (j=startj+1; j<=endj; j++) {
        numBlackPels = 0;
        for(x=left; x<=right; x++) {
            for(y=j; y<=j+kernelWidth; y++) {
                l_int32 retval = pixGetPixel(pixg, x, y, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
        }
        //printf("T %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<5) {
            //printf("break!\n");
            return j;
        }

    }

    return startj;
    
}

/// RemoveBlackPelsBlockRowBot()
///____________________________________________________________________________

l_uint32 RemoveBlackPelsBlockRowBot(PIX *pixg, l_uint32 startj, l_uint32 endj, l_uint32 left, l_uint32 right, l_uint32 kernelWidth, l_uint32 blackThresh) {
    l_uint32 j;
    l_uint32 a;

    l_uint32 numBlackPels=0;

    numBlackPels = 0;
    l_uint32 x, y;

    for (j=startj+1; j>=endj; j--) {
        numBlackPels = 0;
        for(x=left; x<=right; x++) {
            for(y=j; y<=j+kernelWidth; y++) {
                l_int32 retval = pixGetPixel(pixg, x, y, &a);
                assert(0 == retval);
                if (a<blackThresh) {
                    numBlackPels++;
                }
            }
        }
        //printf("B %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<5) {
            //printf("break!\n");
            return j;
        }

    }

    return startj;
    
}

/// RemoveBackgroundTop()
///____________________________________________________________________________
l_int32 RemoveBackgroundTop(PIX *pixg, l_int32 rotDir) {
    

    l_uint32 w = pixGetWidth(pixg);
    l_uint32 h = pixGetHeight(pixg);
    l_uint32 a;


    l_uint32 limitL, limitR, limitB;

    if (1 == rotDir) {
        limitL = (l_uint32)(0.20*w);
        limitR = w-1;
    } else if (-1 == rotDir) {
        limitL = 1;
        limitR = (l_uint32)(0.80*w);
    } else {
        assert(0);
    }

    limitB = l_uint32(0.80*h);
    printf("T: limitL=%d, limitR=%d, limitB=%d\n", limitL, limitR, limitB);
    l_int32 initialBlackThresh = 140;
    l_uint32 numBlackRequired   = (l_uint32)(0.90*(limitR-limitL));

    l_uint32 i, j;

    for(j=0; j<=limitB; j++) {
    
        l_uint32 numBlackPels = 0;
        for (i=limitL; i<=limitR; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            if (a<initialBlackThresh) {
                numBlackPels++;
            }
        }
        printf("T %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            printf("break!\n");
            return j;
        }
    }

    return 0;

}

/// RemoveBackgroundBottom()
///____________________________________________________________________________
l_int32 RemoveBackgroundBottom(PIX *pixg, l_int32 rotDir) {
    

    l_uint32 w = pixGetWidth(pixg);
    l_uint32 h = pixGetHeight(pixg);
    l_uint32 a;


    l_int32 limitL, limitR, limitT;

    if (1 == rotDir) {
        limitL = (l_uint32)(w*0.20);
        limitR = w-1;
    } else if (-1 == rotDir) {
        limitL = 1;
        limitR = (l_uint32)(w*0.80);
    } else {
        assert(0);
    }

    limitT = l_uint32(0.20*h);
    printf("B: limitL=%d, limitR=%d, limitT=%d\n", limitL, limitR, limitT);

    l_int32 initialBlackThresh = 140;
    l_uint32 numBlackRequired   = (l_uint32)(0.90*(limitR-limitL));

    l_int32 i, j;

    for(j=h-1; j>=limitT; j--) {
    
        l_uint32 numBlackPels = 0;
        for (i=limitL; i<=limitR; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            if (a<initialBlackThresh) {
                numBlackPels++;
            }
        }
        printf("B %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            printf("break!\n");
            return j;
        }
    }

    return h-1;

}

/// RemoveBackgroundOuter_L()
///____________________________________________________________________________
l_int32 RemoveBackgroundOuter_L(PIX *pixg, l_int32 iStart, l_int32 iEnd, l_int32 limitT, l_int32 limitB, l_int32 blackThresh, l_uint32 numBlackRequired) {
    

    l_uint32 a;


    l_int32 i, j;

    for(i=iStart; i<=iEnd; i++) {
    
        l_uint32 numBlackPels = 0;
        for (j=limitT; j<=limitB; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            if (a<blackThresh) {
                numBlackPels++;
            }
        }
        printf("O %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            printf("break! (thresh=%d)\n", numBlackRequired);
            return i;
        }
    }

    return iStart;

}
/// RemoveBackgroundOuter_R()
///____________________________________________________________________________
l_int32 RemoveBackgroundOuter_R(PIX *pixg, l_int32 iStart, l_int32 iEnd, l_int32 limitT, l_int32 limitB, l_int32 blackThresh, l_uint32 numBlackRequired) {
    

    l_uint32 a;


    l_int32 i, j;

    for(i=iStart; i>=iEnd; i--) {
    
        l_uint32 numBlackPels = 0;
        for (j=limitT; j<=limitB; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            if (a<blackThresh) {
                numBlackPels++;
            }
        }
        printf("O %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            printf("break! (thresh=%d)\n", numBlackRequired);
            return i;
        }
    }

    return iStart;

}

/// RemoveBackgroundOuter()
///____________________________________________________________________________
l_int32 RemoveBackgroundOuter(PIX *pixg, l_int32 rotDir, l_uint32 topEdge, l_uint32 bottomEdge) {
    

    l_uint32 w = pixGetWidth(pixg);
    l_uint32 h = pixGetHeight(pixg);
    l_uint32 a;

    l_uint32 kernelHeight10 = (l_uint32)(0.10*(bottomEdge-topEdge));

    l_int32 limitT, limitB;
    limitT = topEdge+kernelHeight10;
    limitB = bottomEdge-kernelHeight10;
    l_int32 step;

    l_int32 iStart, iEnd;


    l_int32 initialBlackThresh = 140;
    l_uint32 numBlackRequired   = (l_uint32)(0.90*(limitB-limitT));


    if (1 == rotDir) {
        iStart = w-1;
        iEnd   = (l_int32)(w*0.20);

        printf("O: iStart=%d, iEnd=%d, limitT=%d, limitB=%d\n", iStart, iEnd, limitT, limitB);

        return RemoveBackgroundOuter_R(pixg, iStart, iEnd, limitT, limitB, initialBlackThresh, numBlackRequired);

    } else if (-1 == rotDir) {
        iStart = 0;
        iEnd   = (l_uint32)(w*0.80);

        printf("O: iStart=%d, iEnd=%d, limitT=%d, limitB=%d\n", iStart, iEnd, limitT, limitB);

        return RemoveBackgroundOuter_L(pixg, iStart, iEnd, limitT, limitB, initialBlackThresh, numBlackRequired);

    } else {
        assert(0);
    }

    /*
    l_int32 i, j;

    for(i=iStart; i>=iEnd; i+=step) {
    
        l_uint32 numBlackPels = 0;
        for (j=limitT; j<=limitB; j++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            if (a<initialBlackThresh) {
                numBlackPels++;
            }
        }
        printf("O %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            printf("break! (thresh=%d)\n", numBlackRequired);
            return i;
        }
    }

    return iStart;
    */
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
    pixWrite("/home/rkumar/public_html/outgray.jpg", pixg, IFF_JFIF_JPEG); 
    pixWrite("/home/rkumar/public_html/out.jpg", pixd, IFF_JFIF_JPEG); 

    #if DEBUGMOV
    {
        char cmd[512];
        debugmov.outDir = "/home/rkumar/public_html/debugmov";
        debugmov.framenum =-1;
        debugmov.filename = basename(filein);
        pixWrite("/home/rkumar/public_html/debugmov/smallgray.jpg", pixg, IFF_JFIF_JPEG); 
        int ret = snprintf(cmd, 512, "rm -rf %s/frames", debugmov.outDir);
        assert(ret);
        ret = system(cmd);
        assert(0 == ret);

        ret = snprintf(cmd, 512, "mkdir %s/frames", debugmov.outDir);
        assert(ret);
        ret = system(cmd);
        assert(0 == ret);

        if (1==rotDir) {
            debugmov.edgeBinding = 0;
            debugmov.edgeOuter   = pixGetWidth(pixg) - 1;
        } else if (-1 == rotDir) {
            debugmov.edgeOuter     = 0;
            debugmov.edgeBinding   = pixGetWidth(pixg) - 1;
        } else {
            assert(0);
        }
        
    }
    #endif //DEBUGMOV

    float delta;

    
    l_int32 cropT=-1, cropB=-1, cropR=-1, cropL=-1;
    float deltaT, deltaB, deltaV1, deltaV2, deltaBinding, deltaOuter;
    l_uint32 threshBinding, threshOuter, threshT, threshB;

    /// Do a quick search to find book boundry
    

    /// find binding side edge
/*
    l_int32 bindingEdge = FindBindingEdge(pixg, rotDir, &deltaV1, &threshBinding);
    
    if (-1 == bindingEdge) {
        printf("COULD NOT FIND BINDING!");
    } else {
        printf("binding edge= %d\n", bindingEdge);
    }
    printf("binding edge threshold is %d\n", threshBinding);
*/
    /// find top edge
    //l_int32 topEdge = FindHorizontalEdge(pixg, rotDir, bindingEdge, 0, &deltaT, &threshT);
    l_int32 topEdge = RemoveBackgroundTop(pixg, rotDir);

    /// find bottom edge
    //l_int32 bottomEdge = FindHorizontalEdge(pixg, rotDir, bindingEdge, 1, &deltaB, &threshB);
    l_int32 bottomEdge = RemoveBackgroundBottom(pixg, rotDir);

    assert(bottomEdge>topEdge);

l_int32 bindingEdge = FindBindingEdge2(pixg, rotDir, topEdge, bottomEdge, &deltaBinding, &threshBinding);
if (-1 == bindingEdge) {
    printf("COULD NOT FIND BINDING!");
} else {
    printf("binding edge= %d\n", bindingEdge);
}
printf("binding edge threshold is %d\n", threshBinding);

    /// find the outer vertical edge
    //l_int32 outerEdge = FindOuterEdge(pixg, rotDir, &deltaV2, &threshOuter);
    l_int32 outerEdge = RemoveBackgroundOuter(pixg, rotDir, topEdge, bottomEdge);
   

    cropT = topEdge*8;
    cropB = bottomEdge*8;
    if (1 == rotDir) {
        cropL = bindingEdge*8;
        cropR = outerEdge*8;
    } else if (-1 == rotDir) {
        cropR = bindingEdge*8;
        cropL = outerEdge*8;
    } else {
        //FIXME deal with rotDir=0
        assert(0);
    }

    printf("in main: cL=%d, cR=%d, cT=%d, cB=%d\n", cropL, cropR, cropT, cropB);

    /// Now that we have the crop box, use Postl's meathod for deskew
    double skewScore, skewConf;
    //Deskew(pixg, cropL, cropR, cropT, cropB, &skewScore, &skewConf);

    PIX *pixBig;

    startTimer();
    if ((pixBig = pixRead(filein)) == NULL) {
       exit(ERROR_INT("pixBig not made", mainName, 1));
    }
    printf("opened large jpg in %7.3f sec\n", stopTimer());

    PIX *pixBigG = pixConvertRGBToGray (pixBig, 0.30, 0.60, 0.10);
    PIX *pixBigR = pixRotate90(pixBigG, rotDir);
    BOX *box     = boxCreate(cropL, cropT, cropR-cropL, cropB-cropT);
    PIX *pixBigC = pixClipRectangle(pixBigR, box, NULL);
    PIX *pixBigB = pixThresholdToBinary (pixBigC, threshBinding);    
    pixWrite("/home/rkumar/public_html/outbin.png", pixBigB, IFF_PNG); 

    l_float32    angle, conf, textAngle;

    printf("calling pixFindSkew\n");
    if (pixFindSkew(pixBigB, &textAngle, &conf)) {
      /* an error occured! */
        printf("angle=%.2f\nconf=%.2f\n", 0.0, -1.0);
     } else {
        printf("angle=%.2f\nconf=%.2f\n", textAngle, conf);
    }   

    //Deskew(pixbBig, cropL*8, cropR*8, cropT*8, cropB*8, &skewScore, &skewConf);
    
    if (conf >= 2.0) {
        debugstr("skewMode: text\n");
        angle = textAngle;
    } else {

        debugstr("skewMode: edge\n");
        printf("deltaBinding = %f\n", deltaBinding);
        //angle = (deltaT + deltaB + deltaV1 + deltaV2)/4;
        angle = deltaBinding; //TODO: calculate average of four edge deltas.
    }
    
    printf("rotating bigR by %f\n", angle);

    PIX *pixBigR2 = pixRotate90(pixBigG, rotDir);
    //TODO: why does this segfault when passing in pixBigR?
    PIX *pixBigT = pixRotate(pixBigR2,
                    deg2rad*angle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);



    //AdjustCropBox(pixBigT, &cropL, &cropR, &cropT, &cropB, 8*5);
    //AdjustCropBoxByVariance(pixBigT, &cropL, &cropR, &cropT, &cropB, 3, angle);

    printf("finding clean lines...\n");
    l_int32 w = pixGetWidth(pixBigT);
    l_int32 h = pixGetHeight(pixBigT);
    //cropR = removeBlackPelsColRight(pixBigT, cropR, (int)(w*0.75), cropT, cropB);
    l_int32 limitLeft = calcLimitLeft(w,h,angle);
    l_int32 limitTop  = calcLimitTop(w,h,angle);

    l_uint32 left, right;
    l_uint32 threshL, threshR;
    if (1==rotDir) {
        left  = cropL;
        right = cropL+2*limitLeft;
        threshL = threshBinding;
        threshR = threshBinding; //threshOuter; //binding thresh works better
    } else if (-1 == rotDir) {
        left  = cropL;
        right = (l_uint32)(w*0.25);
        threshL = threshBinding; //threshOuter; //binding thresh works better
        threshR = threshBinding;
    } else {
        assert(0);
    }
    cropL = RemoveBlackPelsBlockColLeft(pixBigT, left, right, cropT, cropB, 3, threshL);

    if (1==rotDir) {
        left  = (int)(w*0.75);
        right = cropR;
    } else if (-1 == rotDir) {
        left  = cropR-2*limitLeft;
        right = cropR;
    } else {
        assert(0);
    }
    printf("bigW=%d, bigH=%d\n", w, h);
    cropR = RemoveBlackPelsBlockColRight(pixBigT, right, left, cropT, cropB, 3, threshR);

    cropT = RemoveBlackPelsBlockRowTop(pixBigT, cropT, cropT+2*limitTop, cropL, cropR, 3, threshT);
    cropB = RemoveBlackPelsBlockRowBot(pixBigT, cropB, cropB-2*limitTop, cropL, cropR, 3, threshB);

    //pixWrite("/home/rkumar/public_html/outbig.jpg", pixBigT, IFF_JFIF_JPEG); 
    //PIX *pixTmp = pixThresholdToBinary (pixBigT, threshBinding);    
    //pixWrite("/home/rkumar/public_html/outbin.png", pixTmp, IFF_PNG); 

    printf("adjusted: cL=%d, cR=%d, cT=%d, cB=%d\n", cropL, cropR, cropT, cropB);
    BOX *boxCrop = boxCreate(cropL/8, cropT/8, (cropR-cropL)/8, (cropB-cropT)/8);

    PIX *pixFinalR = pixRotate(pixd,
                    deg2rad*angle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    pixRenderBoxArb(pixFinalR, boxCrop, 1, 255, 0, 0);
    pixWrite("/home/rkumar/public_html/outbox.jpg", pixFinalR, IFF_JFIF_JPEG); 

    PIX *pixFinalR2 = pixRotate(pixd,
                    deg2rad*angle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    PIX *pixFinalC = pixClipRectangle(pixFinalR2, boxCrop, NULL);
    pixWrite("/home/rkumar/public_html/outcrop.jpg", pixFinalC, IFF_JFIF_JPEG); 

    /// cleanup
    pixDestroy(&pixg);
    pixDestroy(&pixs);
    pixDestroy(&pixd);
}
