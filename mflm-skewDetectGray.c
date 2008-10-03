/*
Copyright(c)2007 Internet Archive. Software license GPL version 2.

linked against Leptonica 1.37

compile with:
g++ -ansi -Werror -Wall -D_BSD_SOURCE -DANSI -fPIC -O3  -I../src -I/usr/X11R6/include  -DL_LITTLE_ENDIAN -o mflm-skewDetectGray mflm-skewDetectGray.c ../lib/nodebug/liblept.a -ltiff -ljpeg -lpng -lz -lm

run with:
mflm-skewDetectGray filein.jpg

*/



#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"

#include <sys/stat.h>   //for stat()
#include <sys/types.h>  //for stat()
#include <sys/errno.h>  //for stat()

#include <assert.h>

#define debugstr printf
//#define debugstr

typedef unsigned int    UINT;
typedef unsigned int    UINT32;

int PaperLuminosity ( PIX *pix, int *pThreshold );

int main(int    argc,
     char **argv)
{
    
    PIX         *pixin;
    char        *filein;
    int         srcH, srcW;
    static char  mainName[] = "mflm-skewDetectGray";
    l_float32    angle, conf;

    if (argc != 2) {
        exit(ERROR_INT(" Syntax:  mflm-skewDetectGray grayscaleinput.jpg",
                         mainName, 1));
    }
    
    filein   = argv[1];

    if (NULL == (pixin = pixRead(filein))) {
        exit(ERROR_INT("input jpeg could not be read", mainName, 1));
    }
    
    l_int32 depth = pixGetDepth(pixin);
    assert(8 == depth);


    srcW = pixGetWidth(pixin);
    srcH = pixGetHeight(pixin);

    debugstr("sourceWidth = %d\n", srcW);
    debugstr("sourceHeight = %d\n", srcH);

    if ((srcW<4) | (srcH<4)) {
        //Binary search downsamples image by 4
        printf("pixFindSkew: angle = %.2f, conf = %.2f\n", 0.0, -1.0);
        return 0;
    }

    NUMA *hist = pixGrayHistogram(pixin);
    //printf("colors: %d\n", numaGetCount(hist));
    assert(256 == numaGetCount(hist));
    int i;
    
    /*
    for (i=0; i<255; i++) {
        int dummy;
        numaGetIValue(hist, i, &dummy);
        printf("%d: %d\n", i, dummy);
    }
    */
    
    int numPels = srcW*srcH;
    printf("numPels = %d\n", numPels);
    
    float acc=0;
    for (i=255; i>=140; i--) {
        float dummy;
        numaGetFValue(hist, i, &dummy);
        acc+=dummy;
        //printf("%d: %f\n", i, 100*acc/numPels);
        
        if (0.80<(acc/numPels)) {
            break;
        }    
    }

    printf ("threshold = %d\n", i);


    PIX *pixb = pixThresholdToBinary(pixin, i);    
    //pixWrite("/home/rkumar/public_html/outbin.png", pixb, IFF_PNG); 

    if (pixFindSkew(pixb, &angle, &conf)) {
      /* an error occured! */
        printf("pixFindSkew: angle = %.2f, conf = %.2f\n", 0.0, -1.0);
     } else {
        printf("pixFindSkew: angle = %.2f, conf = %.2f\n", angle, conf);
    }   
    
  
    return 0;

}
