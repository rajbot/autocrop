#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"
#include <math.h>   //for sqrt
#include <assert.h>
#include <float.h>  //for DBL_MAX
#include <limits.h> //for INT_MAX
#include "autoCropCommon.h"
#include "autocrop_remove_bg.h"

/*  This file contains quick-and-dirty background removal code. It operates on images
    which have already been bitonalized. We are using these functions with low resolution
    images (scaled down by 8x) in order to set a rough crop box.

    These functions first set an inital crop box based on 80% of the width of the image
    and 100% of the height. We set the inital crop to the outer edge of the book,
    since there might be bookmarks and other pieces of paper inserted on the binding side.

    We then shrink the initial crop box by removing black lines. We consider a line to
    be black if 90% of the pixels in the line are black.
*/


/// remove_bg_top()
///____________________________________________________________________________
l_int32 remove_bg_top(PIX *pixb, l_int32 rotDir) {

    l_int32    w, h, d;
    pixGetDimensions(pixb, &w, &h, &d);
    assert(pixGetDepth(pixb) == 1);

    l_uint32 a;
    l_uint32 limitL, limitR, limitB;

    if (1 == rotDir) {
        limitL = (l_uint32)(0.20*w);
        limitR = w-1;
    } else if (-1 == rotDir) {
        limitL = 1;
        limitR = (l_uint32)(0.80*w);
    } else if (0 == rotDir) {
        limitL = 1;
        limitR = w-1;
    } else {
        assert(0);
    }

    limitB = l_uint32(0.80*h);
    //printf("T: limitL=%d, limitR=%d, limitB=%d\n", limitL, limitR, limitB);

    //number of black pels required for this line to be considered part of the background
    l_uint32 numBlackRequired   = (l_uint32)(0.90*(limitR-limitL));

    l_uint32 i, j;

    for(j=0; j<=limitB; j++) {

        l_uint32 numBlackPels = 0;
        for (i=limitL; i<=limitR; i++) {
            l_int32 retval = pixGetPixel(pixb, i, j, &a);
            assert(0 == retval);
            if (PEL_IS_BLACK == a) {
                numBlackPels++;
            }
        }
        //printf("T %d: numBlack=%d\n", j, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            //printf("break at %d!\n", j);
            return j;
        }
    }

    return 0;

}


/// remove_bg_bottom()
///____________________________________________________________________________
l_int32 remove_bg_bottom(PIX *pixb, l_int32 rotDir) {

    l_int32    w, h, d;
    pixGetDimensions(pixb, &w, &h, &d);
    assert(pixGetDepth(pixb) == 1);

    l_uint32 a;
    l_int32 limitL, limitR, limitT;

    if (1 == rotDir) {
        limitL = (l_uint32)(w*0.20);
        limitR = w-1;
    } else if (-1 == rotDir) {
        limitL = 1;
        limitR = (l_uint32)(w*0.80);
    } else if (0 == rotDir) {
        limitL = 1;
        limitR = w-1;
    } else {
        assert(0);
    }

    limitT = l_uint32(0.20*h);
    //printf("B: limitL=%d, limitR=%d, limitT=%d\n", limitL, limitR, limitT);

    //number of black pels required for this line to be considered part of the background
    l_uint32 numBlackRequired   = (l_uint32)(0.90*(limitR-limitL));

    l_int32 i, j;

    for(j=h-1; j>=limitT; j--) {

        l_uint32 numBlackPels = 0;
        for (i=limitL; i<=limitR; i++) {
            l_int32 retval = pixGetPixel(pixb, i, j, &a);
            assert(0 == retval);
            if (PEL_IS_BLACK == a) {
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


/// remove_bg_outer_L()
///____________________________________________________________________________
l_int32 remove_bg_outer_L(PIX *pixb, l_int32 iStart, l_int32 iEnd, l_int32 limitT, l_int32 limitB, l_uint32 numBlackRequired) {

    l_uint32 a;
    l_int32 i, j;

    for(i=iStart; i<=iEnd; i++) {

        l_uint32 numBlackPels = 0;
        for (j=limitT; j<=limitB; j++) {
            l_int32 retval = pixGetPixel(pixb, i, j, &a);
            assert(0 == retval);
            if (PEL_IS_BLACK == a) {
                numBlackPels++;
            }
        }
        //debugstr("O %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            debugstr("remove_bg_outer_L break! (thresh=%d)\n", numBlackRequired);
            return i;
        }
    }

    return iStart;

}


/// remove_bg_outer_R()
///____________________________________________________________________________
l_int32 remove_bg_outer_R(PIX *pixb, l_int32 iStart, l_int32 iEnd, l_int32 limitT, l_int32 limitB, l_uint32 numBlackRequired) {

    l_uint32 a;
    l_int32 i, j;

    for(i=iStart; i>=iEnd; i--) {

        l_uint32 numBlackPels = 0;
        for (j=limitT; j<=limitB; j++) {
            l_int32 retval = pixGetPixel(pixb, i, j, &a);
            assert(0 == retval);
            if (PEL_IS_BLACK == a) {
                numBlackPels++;
            }
        }
        debugstr("R %d: numBlack=%d\n", i, numBlackPels);
        if (numBlackPels<numBlackRequired) {
            debugstr("remove_bg_outer_R break! (thresh=%d)\n", numBlackRequired);
            return i;
        }
    }

    return iStart;

}


/// remove_bg_outer()
///____________________________________________________________________________
l_int32 remove_bg_outer(PIX *pixb, l_int32 rotDir, l_uint32 topEdge, l_uint32 bottomEdge) {


    l_int32 w, h, d;
    pixGetDimensions(pixb, &w, &h, &d);
    assert(pixGetDepth(pixb) == 1);

    l_uint32 kernelHeight10 = (l_uint32)(0.10*(bottomEdge-topEdge));

    l_int32 limitT, limitB;
    limitT = topEdge+kernelHeight10;
    limitB = bottomEdge-kernelHeight10;
    l_int32 step;

    l_int32 iStart, iEnd;


    //l_int32 initialBlackThresh = 140;
    l_uint32 numBlackRequired   = (l_uint32)(0.90*(limitB-limitT));


    if (1 == rotDir) {
        iStart = w-1;
        iEnd   = (l_int32)(w*0.20);

        debugstr("R: iStart=%d, iEnd=%d, limitT=%d, limitB=%d\n", iStart, iEnd, limitT, limitB);

        return remove_bg_outer_R(pixb, iStart, iEnd, limitT, limitB, numBlackRequired);

    } else if (-1 == rotDir) {
        iStart = 0;
        iEnd   = (l_uint32)(w*0.80);

        debugstr("L: iStart=%d, iEnd=%d, limitT=%d, limitB=%d\n", iStart, iEnd, limitT, limitB);

        return remove_bg_outer_L(pixb, iStart, iEnd, limitT, limitB, numBlackRequired);

    } else {
        assert(0);
    }

}