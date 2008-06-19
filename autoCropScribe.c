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

#define debugstr printf
//#define debugstr


/// CalculateSADcol()
/// calculate sum of absolute differences of two rows of adjacent column
/// last SAD calculation is for row j=right-1 and j=right.
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
    printf("jTop/Bot is %d/%d\n", jTop, jBot);

    for (i=left; i<right; i++) {
        printf("%d: ", i);
        acc=0;
        for (j=jTop; j<jBot; j++) {
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


/// FindGutterCrop()
/// This funciton finds the gutter-side (binding-side) crop line.
/// TODO: The return value should indicate a confidence.
///____________________________________________________________________________
l_uint32 FindGutterCrop(PIX *pixg, l_int32 rotDir) {

    //Currently, we can only do right-hand leafs
    assert(1 == rotDir);


    //Assume we can find the binding within the first 10% of the image width
    l_uint32 width10 = (l_uint32)(pixGetWidth(pixg) * 0.10);

    l_int32    reti;
    l_uint32   retDiff;
    CalculateSADcol(pixg, 0, width10, 0.10, &reti, &retDiff);

    return 1;
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

    FindGutterCrop(pixg, rotDir);

    /// cleanup
    pixDestroy(&pixs);
    pixDestroy(&pixd);
}
