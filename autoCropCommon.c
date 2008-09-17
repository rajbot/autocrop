#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"
#include <math.h>   //for sqrt
#include <assert.h>

#define debugstr printf
//#define debugstr

static const l_float32  deg2rad            = 3.1415926535 / 180.;

l_int32 min_int32(l_int32 a, l_int32 b) {
    return b + ((a-b) & (a-b)>>31);
}

l_int32 max_int32(l_int32 a, l_int32 b) {
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
            //printf("acc: %d\n", acc);
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
        //printf("%d: ", j);
        acc=0;
        for (i=left; i<right; i++) {
            l_int32 retval = pixGetPixel(pixg, i, j, &a);
            assert(0 == retval);
            retval = pixGetPixel(pixg, i, j+1, &b);
            assert(0 == retval);
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

/// CalculateTreshInitial ()
///____________________________________________________________________________

l_int32 CalculateTreshInitial(PIX *pixg, l_int32 *histmax) {
        NUMA *hist = pixGetGrayHistogram(pixg, 1);
        assert(NULL != hist);
        assert(256 == numaGetCount(hist));
        int i;
        
//         for (i=0; i<255; i++) {
//             int dummy;
//             numaGetIValue(hist, i, &dummy);
//             printf("init hist: %d: %d\n", i, dummy);
//         }

        l_int32 brightestPel;
        for (i=255; i>=0; i--) {
            float dummy;
            numaGetFValue(hist, i, &dummy);
            if (dummy > 0) {
                brightestPel = i;
                break;
            }
        }

        l_int32 darkestPel;
        for (i=0; i<=255; i++) {
            float dummy;
            numaGetFValue(hist, i, &dummy);
            if (dummy > 0) {
                darkestPel = i;
                break;
            }
        }
        
        l_int32 limit = (brightestPel-darkestPel)/2;
        
        printf("brighestPel=%d, darkestPel=%d, limit=%d\n", brightestPel, darkestPel, limit);
        
        float peak = 0;
        l_int32 peaki;
        for (i=255; i>=limit; i--) {
            float dummy;
            numaGetFValue(hist, i, &dummy);
            if (dummy > peak) {
                peak = dummy;
                peaki = i;
            }
        }
        //printf("hist peak at i=%d with val=%f\n", peaki, peak);
        
        l_int32 thresh = -1;
        float threshLimit = peak * 0.2;
        //printf("thresh limit = %f\n", threshLimit);
        for (i=peaki-1; i>0; i--) {
            float dummy;
            numaGetFValue(hist, i, &dummy);
            if (dummy<threshLimit) {
                thresh = i;
                break;
            }
        }
        
        if (-1 == thresh) {
            thresh = peaki >>1;
        }
        
        if (0 == thresh) {
            //this could be a plain black img.
            thresh = 140;
        }

        //printf("init thresh at i=%d\n", thresh);
        *histmax = peaki;
        return thresh;
}

/// CalculateNumBlackPelsRow
///____________________________________________________________________________
l_int32 CalculateNumBlackPelsRow(PIX *pixg, l_int32 j, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh) {
    l_int32 numBlackPels = 0;
    l_int32 i;
    l_uint32 a;

    for (i=limitL; i<=limitR; i++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }

    return numBlackPels;
}

/// CalculateNumBlackPelsCol
///____________________________________________________________________________
l_int32 CalculateNumBlackPelsCol(PIX *pixg, l_int32 i, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh) {
    l_int32 numBlackPels = 0;
    l_int32 j;
    l_uint32 a;

    for (j=limitT; j<=limitB; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<blackThresh) {
            numBlackPels++;
        }
    }

    return numBlackPels;
}

/// FindBlackBar()
///____________________________________________________________________________
l_int32 FindBlackBar(PIX *pixg, 
                  l_int32 left, 
                  l_int32 right,
                  l_int32 h,
                  l_int32 thresh,
                  l_int32 *bindingEdgeL, 
                  l_int32 *bindingEdgeR)
{
    l_int32 i;
    l_int32 gotEdgeL = 0;
    l_int32 gotEdgeR = 0;

    for (i=left; i<=right; i++) {
        //printf("%d (%d): ", i, i*8);

        l_int32 numBlackPels = CalculateNumBlackPelsCol(pixg, i, 0, h-1, thresh);
        //printf("numBlackPels=%d, h=%d thresh=%d", numBlackPels, h, thresh);
        if (numBlackPels == h) {
            *bindingEdgeL = i;
            gotEdgeL      = 1;
            //printf("FOUND!\n");
            break;
        }
        //printf("\n");
    }

    for (i=right; i>=left; i--) {
        //printf("%d: ", i);

        l_int32 numBlackPels = CalculateNumBlackPelsCol(pixg, i, 0, h-1, thresh);
        //printf("numBlackPels=%d, h=%d ", numBlackPels, h);
        if (numBlackPels == h) {
            *bindingEdgeR = i;
            gotEdgeR      = 1;
            //printf("FOUND!\n");
            break;
        }
        //printf("\n");
    }

    if (!(gotEdgeL & gotEdgeR)) {
        return -1;
        //assert(0);
    }

    return 1;
}

/// FindBlackBarAndThresh()
///____________________________________________________________________________
void FindBlackBarAndThresh(PIX *pixg, 
                  l_int32 left, 
                  l_int32 right,
                  l_int32 h,
                  l_int32 *barEdgeL, 
                  l_int32 *barEdgeR,
                  l_int32 *barThresh)

{
    //pixWrite("findblackbar.jpg", pixg, IFF_JFIF_JPEG);

    l_int32 histmax;
    l_int32 darkThresh = CalculateTreshInitial(pixg, &histmax);
    l_int32 thresh;

    for (thresh = darkThresh; thresh<histmax; thresh++) {
        l_int32 blackBarL, blackBarR;
        printf("thresh=%d ", thresh);
        l_int32 retval = FindBlackBar(pixg, left, right, h, thresh, &blackBarL, &blackBarR);
        if (-1 == retval) continue;

        l_int32 barWidth = blackBarR - blackBarL;
        printf("L=%d, R=%d, W=%d\n", blackBarL, blackBarR, barWidth);
        if (barWidth >= 1) {
            *barEdgeL = blackBarL;
            *barEdgeR = blackBarR;
            *barThresh = thresh;
            return;
        }
    }

    assert(0);
}



/// FindBindingEdge2()
///____________________________________________________________________________
l_int32 FindBindingEdge2(PIX      *pixg,
                         l_int32  rotDir,
                         l_uint32 topEdge,
                         l_uint32 bottomEdge,
                         float    *skew,
                         l_uint32 *thesh,
                         l_int32 textBlockL,
                         l_int32 textBlockR)
{
    //pixWrite("findbinding.jpg", pixg, IFF_JFIF_JPEG);

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

    l_uint32 left, right;
    if (1 == rotDir) {
        left  = 0;
        if (-1 == textBlockL) {
            right = width10;
        } else {
            right = textBlockL;
        }
    } else {
        if (-1 == textBlockR) {
            left  = w - width10;
        } else {
            left = textBlockR;
        }
        right = w - 1;
    }
    
    l_int32    bindingEdge;// = -1;
    l_uint32   bindingEdgeDiff;// = 0;
    float      bindingDelta = 0.0;

    printf("left = %d, right=%d, textBlockR = %d, width = %d, width10=%d\n", left, right, textBlockR, w, width10);
    l_int32 blackBarL, blackBarR;
    l_int32 histmax;
    l_int32 darkThresh; // = CalculateTreshInitial(pixg, &histmax);
    //FindBlackBar(pixg, left, right, h, darkThresh, &blackBarL, &blackBarR);
    FindBlackBarAndThresh(pixg, left, right, h, &blackBarL, &blackBarR, &darkThresh);
    printf("init blackBar L=%d, R=%d, width=%d, thresh=%d\n", blackBarL, blackBarR, blackBarR-blackBarL, darkThresh);

    //CalculateSADcol(pixg, left, right, jTop, jBot, &bindingEdge, &bindingEdgeDiff);
    CalculateSADcol(pixg, blackBarL, blackBarR, jTop, jBot, &bindingEdge, &bindingEdgeDiff);
    //printf("init bindingEdge=%d, diff=%d\n", bindingEdge*8, bindingEdgeDiff);

    float delta;
    //0.05 degrees is a good increment for the final search
    for (delta=-1.0; delta<=1.0; delta+=0.05) {
    
        if ((delta>-0.01) && (delta<0.01)) { continue;}
        
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

        //l_uint32 left, right;
        if (1 == rotDir) {
            left  = limitLeft;
            if (-1 == textBlockL) {
                right = width10;
            } else {
                right = textBlockL;
            }
        } else {
            if (-1 == textBlockR) {
                left  = w - width10;
            } else {
                left = textBlockR;
            }
            right = w - limitLeft-1;
        }

        FindBlackBar(pixg, left, right, h, darkThresh, &blackBarL, &blackBarR);
        //printf("blackBar L=%d, R=%d, width=%d\n", blackBarL, blackBarR, blackBarR-blackBarL);

        //CalculateSADcol(pixt, left, right, jTop, jBot, &strongEdge, &strongEdgeDiff);
        CalculateSADcol(pixt, blackBarL, blackBarR, jTop, jBot, &strongEdge, &strongEdgeDiff);

        //printf("delta=%f, strongest edge of gutter is at i=%d with diff=%d, w,h=(%d,%d)\n", delta, strongEdge, strongEdgeDiff, w, h);
        if (strongEdgeDiff > bindingEdgeDiff) {
            bindingEdge = strongEdge;
            bindingEdgeDiff = strongEdgeDiff;
            bindingDelta = delta;
            //printf("setting best delta to %f\n", bindingDelta);
            #if DEBUGMOV
            debugmov.edgeBinding = bindingEdge;
            #endif //DEBUGMOV
        }
        

        pixDestroy(&pixt);    
    }
    
    assert(-1 != bindingEdge); //TODO: handle error
    //printf("BEST: delta=%f, strongest edge of gutter is at i=%d with diff=%d\n", bindingDelta, bindingEdge, bindingEdgeDiff);
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

    l_int32 width3p = (l_int32)(w * 0.03);
    l_int32 rightEdge, leftEdge;
    l_uint32 numBlackLines = 0;
    
    if (bindingLumaA > bindingLumaB) { //found left edge
        l_int32 i;
        l_int32 rightLimit = bindingEdge+width3p;
        rightEdge = bindingEdge; //init this something, in case we never break;
        leftEdge  = bindingEdge;
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
        l_int32 i;
        l_int32 leftLimit = bindingEdge-width3p;
        rightEdge = bindingEdge;
        leftEdge  = bindingEdge; //init this something, in case we never break;
        
        if (leftLimit<0) leftLimit = 0;
        printf("found right edge of gutter, leftLimit=%d, rightLimit=%d\n", leftLimit, bindingEdge-1);
        for (i=bindingEdge-1; i>leftLimit; i--) {
            double lumaAvg = CalculateAvgCol(pixt, i, jTop, jBot);
            printf("i=%d, avg=%f\n", i, lumaAvg);
            if (lumaAvg<threshold) {
                numBlackLines++;
            } else {
                leftEdge = i-1;
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
printf("rightEdge = %d, leftEdge = %d\n", rightEdge, leftEdge);
    if ((numBlackLines >=1) && (numBlackLines<width3p)) {
        if (1 == rotDir) {
            return rightEdge;
        } else if (-1 == rotDir) {
            return leftEdge;
        } else {
            assert(0);
        }
    } else {
        debugstr("COULD NOT FIND BINDING, using strongest edge!\n");
        return bindingEdge;
    }    
    
    return 1; //TODO: return error code on failure
}



/// FindBindingUsingBlackBar()
///____________________________________________________________________________
l_int32 FindBindingUsingBlackBar(PIX      *pixg,
                                 l_int32  rotDir,
                                 l_int32 topEdge,
                                 l_int32 bottomEdge,
                                 l_int32 textBlockL,
                                 l_int32 textBlockR)
{
    //Currently, we can only do right-hand leafs
    assert((1 == rotDir) || (-1 == rotDir));

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    l_uint32 width10 = (l_uint32)(w * 0.10);
    l_int32 histmax;
    l_int32 darkThresh = CalculateTreshInitial(pixg, &histmax);

    l_int32 kernelHeight10 = (l_uint32)(0.10*(bottomEdge-topEdge));
    //l_uint32 jTop = (l_uint32)((1-kKernelHeight)*0.5*h);
    //l_uint32 jBot = (l_uint32)((1+kKernelHeight)*0.5*h);    
    l_int32 jTop = topEdge+kernelHeight10;
    l_int32 jBot = bottomEdge-kernelHeight10;

    l_uint32 left, right;
    if (1 == rotDir) {
        left  = 0;
        if (-1 == textBlockL) {
            right = width10;
        } else {
            right = textBlockL;
        }
    } else {
        if (-1 == textBlockR) {
            left  = w - width10;
        } else {
            left = textBlockR;
        }
        right = w - 1;
    }

    l_int32    bindingEdge;// = -1;
    l_uint32   bindingEdgeDiff;// = 0;
    l_int32    blackBarL, blackBarR;// = -1;
    l_int32    bindingWidth;// = 0;
    float      bindingDelta = 0.0;
    FindBlackBar(pixg, left, right, h, darkThresh, &blackBarL, &blackBarR);
    printf("init blackBar L=%d, R=%d, width=%d\n", blackBarL, blackBarR, blackBarR-blackBarL);


    CalculateSADcol(pixg, blackBarL, blackBarR, jTop, jBot, &bindingEdge, &bindingEdgeDiff);
    printf("init bindingEdge=%d, diff=%d\n", bindingEdge, bindingEdgeDiff);


    //if (1 == rotDir) {
    //    return bindingEdgeL;
    //} else {
    //    return bindingEdgeR;
    //}
    return 1;
}

/// ConvertToGray()
///____________________________________________________________________________
PIX* ConvertToGray(PIX *pix) {

    PIX *pixg;
    l_int32 maxchannel;
    l_int32 useSingleChannelForGray = 0;

    NUMA *histR, *histG, *histB;
    l_int32 ret = pixGetColorHistogram(pix, 1, &histR, &histG, &histB);
    assert(0 == ret);
    
    l_float32 maxval;
    l_int32   maxloc[3];
    
    ret = numaGetMax(histR, &maxval, &maxloc[0]);
    assert(0 == ret);
    
    printf("red peak at %d with val %f\n", maxloc[0], maxval);
    
    ret = numaGetMax(histG, &maxval, &maxloc[1]);
    assert(0 == ret);
    printf("green peak at %d with val %f\n", maxloc[1], maxval);
    
    ret = numaGetMax(histB, &maxval, &maxloc[2]);
    assert(0 == ret);
    printf("blue peak at %d with val %f\n", maxloc[2], maxval);
    
    l_int32 i;
    l_int32 max=0, secondmax=0;
    for (i=0; i<3; i++) {
        if (maxloc[i] > max) {
            max = maxloc[i];
            maxchannel = i;
        } else if (maxloc[i] > secondmax) {
            secondmax = maxloc[i];
        }
    }
    printf("max = %d, secondmax=%d\n", max, secondmax);
    if (max > (secondmax*2)) {
        printf("grayMode: SINGLE-channel, channel=%d\n", maxchannel);
        useSingleChannelForGray = 1;
    } else {
        printf("grayMode: three-channel\n");    
    }

    if (useSingleChannelForGray) {
        pixg = pixConvertRGBToGray (pix, (0==maxchannel), (1==maxchannel), (2==maxchannel));
    } else {
        pixg = pixConvertRGBToGray (pix, 0.30, 0.60, 0.10);
    }
    
    return pixg;

}



/// RemoveBackgroundTop()
///____________________________________________________________________________
l_int32 RemoveBackgroundTop(PIX *pixg, l_int32 rotDir, l_int32 initialBlackThresh) {
    

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
    //printf("T: limitL=%d, limitR=%d, limitB=%d\n", limitL, limitR, limitB);
    //l_int32 initialBlackThresh = 140;
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
        //printf("T %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            //printf("break!\n");
            return j;
        }
    }

    return 0;

}

/// RemoveBackgroundBottom()
///____________________________________________________________________________
l_int32 RemoveBackgroundBottom(PIX *pixg, l_int32 rotDir, l_int32 initialBlackThresh) {
    

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
    //printf("B: limitL=%d, limitR=%d, limitT=%d\n", limitL, limitR, limitT);

    //l_int32 initialBlackThresh = 140;
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
        //printf("B %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            //printf("break!\n");
            return j;
        }
    }

    return h-1;

}



/// CalculateMinRow
///____________________________________________________________________________
l_int32 CalculateMinRow(PIX *pixg, l_int32 j, l_int32 limitL, l_int32 limitR) {
    l_int32 i;
    l_uint32 a;
    l_uint32 min = 256; //assume 8-bit image
    for (i=limitL; i<=limitR; i++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<min) {
            min=a;
        }
    }

    assert(min <= 255);
    return min;
}

/// CalculateMinCol
///____________________________________________________________________________
l_int32 CalculateMinCol(PIX *pixg, l_int32 i, l_int32 limitT, l_int32 limitB) {
    l_int32 j;
    l_uint32 a;
    l_uint32 min = 256; //assume 8-bit image
    for (j=limitT; j<=limitB; j++) {
        l_int32 retval = pixGetPixel(pixg, i, j, &a);
        assert(0 == retval);
        if (a<min) {
            min=a;
        }
    }

    assert(min <= 255);
    return min;
}

/// FindDarkRowUp
///____________________________________________________________________________
l_int32 FindDarkRowUp(PIX *pixg, l_int32 limitB, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i, j;
    l_uint32 a;
    l_int32 limitT = max_int32(limitB-((l_int32)(pixGetHeight(pixg)*0.10)), 0);

    for (j=limitB; j>=limitT; j--) {
        l_int32 numBlackPels = CalculateNumBlackPelsRow(pixg, j, limitL, limitR, blackThresh);
        //printf("FindDarkRowUp: j=%d, numBlackPels=%d\n", j, numBlackPels);

        if (numBlackPels > blackLimit) {
            //printf("FindDarkRowUp: returning with row=%d, numBlackPels=%d, blackLimit=%d\n", j, numBlackPels, blackLimit);
            return j;
        }
    }

    return -1; //could not find edge (dark row)
}


/// FindDarkRowDown
///____________________________________________________________________________
l_int32 FindDarkRowDown(PIX *pixg, l_int32 limitT, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i, j;
    l_uint32 a;
    l_int32 h = pixGetHeight(pixg);
    l_int32 limitB = min_int32(limitT+((l_int32(h*0.20))), h-1);

    for (j=limitT; j<=limitB; j++) {
        l_int32 numBlackPels = CalculateNumBlackPelsRow(pixg, j, limitL, limitR, blackThresh);
        //printf("FindDarkRowDown: j=%d, numBlackPels=%d\n", j, numBlackPels);

        if (numBlackPels > blackLimit) {
            //printf("FindDarkRowDown: returning with row=%d, numBlackPels=%d, blackLimit=%d\n", j, numBlackPels, blackLimit);
            return j;
        }
    }

    return -1; //could not find edge (dark row)
}


/// FindWhiteRowUp
///____________________________________________________________________________
l_int32 FindWhiteRowUp(PIX *pixg, l_int32 limitB, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i, j;
    l_uint32 a;
    l_int32 limitT = max_int32(limitB-((l_int32)(pixGetHeight(pixg)*0.10)), 0);

    for (j=limitB; j>=limitT; j--) {
        l_int32 numBlackPels = CalculateNumBlackPelsRow(pixg, j, limitL, limitR, blackThresh);
        //printf("FindWhiteRowUp: j=%d, numBlackPels=%d\n", j, numBlackPels);

        if (numBlackPels <= blackLimit) {
            //printf("FindWhiteRowUp: returning with row=%d, numBlackPels=%d, blackLimit=%d\n", j, numBlackPels, blackLimit);
            return j;
        }
    }

    return -1; //could not find white row
}

/// FindWhiteRowDown
///____________________________________________________________________________
l_int32 FindWhiteRowDown(PIX *pixg, l_int32 limitT, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i, j;
    l_uint32 a;
    l_int32 h = pixGetHeight(pixg);
    l_int32 limitB = min_int32(limitT+((l_int32(h*0.10))), h-1);

    for (j=limitT; j<=limitB; j++) {
        l_int32 numBlackPels = CalculateNumBlackPelsRow(pixg, j, limitL, limitR, blackThresh);
        //printf("FindWhiteRowDown: j=%d, numBlackPels=%d\n", j, numBlackPels);

        if (numBlackPels <= blackLimit) {
            //printf("FindWhiteRowDown: returning with row=%d, numBlackPels=%d, blackLimit=%d\n", j, numBlackPels, blackLimit);
            return j;
        }
    }

    //printf("FindWhiteRowDown: didn't find white row, returning -1\n");
    return -1; //could not find edge (dark row)
}

/// FindDarkColLeft
///____________________________________________________________________________
l_int32 FindDarkColLeft(PIX *pixg, l_int32 limitR, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i;
    l_uint32 a;
    l_int32 limitL = max_int32(limitR-((l_int32)(pixGetWidth(pixg)*0.10)), 0);

    for (i=limitR; i>=limitL; i--) {
        l_int32 numBlackPels = CalculateNumBlackPelsCol(pixg, i, limitT, limitB, blackThresh);
        //printf("FindDarkColLeft: i=%d, numBlackPels=%d\n", i, numBlackPels);

        if (numBlackPels > blackLimit) {
            //printf("FindDarkColLeft: returning with col=%d, numBlackPels=%d, blackLimit=%d\n", i, numBlackPels, blackLimit);
            return i;
        }
    }

    return -1; //could not find edge (dark row)
}

/// FindDarkColRight
///____________________________________________________________________________
l_int32 FindDarkColRight(PIX *pixg, l_int32 limitL, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i;
    l_uint32 a;
    l_int32 w = pixGetWidth(pixg);
    l_int32 limitR = min_int32(limitL+((l_int32)(w*0.10)), w-1);

    for (i=limitL; i<=limitR; i++) {
        l_int32 numBlackPels = CalculateNumBlackPelsCol(pixg, i, limitT, limitB, blackThresh);
        //printf("FindDarkColRight: i=%d, numBlackPels=%d\n", i, numBlackPels);

        if (numBlackPels > blackLimit) {
            //printf("FindDarkColRight: returning with col=%d, numBlackPels=%d, blackLimit=%d\n", i, numBlackPels, blackLimit);
            return i;
        }
    }

    return -1; //could not find edge (dark row)
}

/// FindWhiteColLeft
///____________________________________________________________________________
l_int32 FindWhiteColLeft(PIX *pixg, l_int32 limitR, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i;
    l_uint32 a;
    l_int32 limitL = max_int32(limitR-((l_int32)(pixGetWidth(pixg)*0.10)), 0);

    for (i=limitR; i>=limitL; i--) {
        l_int32 numBlackPels = CalculateNumBlackPelsCol(pixg, i, limitT, limitB, blackThresh);
        //printf("FindWhiteColLeft: i=%d, numBlackPels=%d\n", i, numBlackPels);

        if (numBlackPels <= blackLimit) {
            //printf("FindWhiteColLeft: returning with col=%d, numBlackPels=%d, blackLimit=%d\n", i, numBlackPels, blackLimit);
            return i;
        }
    }

    return -1; //could not white find edge
}

/// FindWhiteColRight
///____________________________________________________________________________
l_int32 FindWhiteColRight(PIX *pixg, l_int32 limitL, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit) {
    l_int32 numBlackPels = 0;
    l_int32 i;
    l_uint32 a;
    l_int32 w = pixGetWidth(pixg);
    l_int32 limitR = min_int32(limitL+((l_int32)(w*0.10)), w-1);

    for (i=limitL; i<=limitR; i++) {
        l_int32 numBlackPels = CalculateNumBlackPelsCol(pixg, i, limitT, limitB, blackThresh);
        //printf("FindWhiteColRight: i=%d, numBlackPels=%d\n", i, numBlackPels);

        if (numBlackPels <= blackLimit) {
            //printf("FindWhiteColRight: returning with col=%d, numBlackPels=%d, blackLimit=%d\n", i, numBlackPels, blackLimit);
            return i;
        }
    }

    return -1; //could not white find edge
}

/// PrintKeyValue_int32
///____________________________________________________________________________
void PrintKeyValue_int32(char *key, l_int32 val) {
    printf("%s: %d\n", key, val);
}

/// DebugKeyValue_int32
///____________________________________________________________________________
void DebugKeyValue_int32(char *key, l_int32 val) {
    #if 1
        PrintKeyValue_int32(key, val);
    #endif
}

/// PrintKeyValue_float
///____________________________________________________________________________
void PrintKeyValue_float(char *key, l_float32 val) {
    printf("%s: %.2f\n", key, val);
}

/// PrintKeyValue_str
///____________________________________________________________________________
void PrintKeyValue_str(char *key, char *val) {
    printf("%s: %s\n", key, val);
}

/// ReduceRowOrCol
///____________________________________________________________________________
void ReduceRowOrCol(l_float32 percent, l_int32 oldMin, l_int32 oldMax, l_int32 *newMin, l_int32 *newMax) {
    assert(oldMax>oldMin);
    l_int32 reduction = (l_int32)(((oldMax-oldMin)>>1)*percent);
    *newMin = oldMin+reduction;
    *newMax = oldMax-reduction;
}
