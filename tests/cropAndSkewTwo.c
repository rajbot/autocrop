/*
Copyright(c)2008 Internet Archive. Software license GPL version 2.

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

static const l_float32  deg2rad            = 3.1415926535 / 180.;

int main(int    argc,
     char **argv)
{

    PIX         *pixs, *pixd;
    char        *filein, *fileout;
    static char  mainName[] = "autoCrop";
    l_float32    scale;
    l_int32      rotDir;
    FILE        *fp;
    FILE        *fpOut;
    int          lum, threshold;
    l_float32    angle, conf;
    
    if (argc != 9) {
        exit(ERROR_INT(" Syntax:  cropAndSkewProxy filein.jpg filout.jpg rotateDirection angle cropx cropy cropw croph",
                         mainName, 1));
    }
    
    filein   = argv[1];
    fileout  = argv[2];
    rotDir   = atoi(argv[3]);

    float skew        = atof(argv[4]);
    l_int32 oldx      = atoi(argv[5]);
    l_int32 oldy      = atoi(argv[6]);
    l_int32 oldw      = atoi(argv[7]);
    l_int32 oldh      = atoi(argv[8]);

    l_int32 newx      = atoi(argv[9]);
    l_int32 newy      = atoi(argv[10]);
    l_int32 neww      = atoi(argv[11]);
    l_int32 newh      = atoi(argv[12]);

    
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



    int w = pixGetWidth( pixd );
    int h = pixGetHeight( pixd );


        
    PIX *pixt = pixRotate(pixd,
                    deg2rad*skew,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);
    BOX *box = boxCreate(oldx, oldy, oldw, oldh);

 
    PIX *pixCrop = pixClipRectangle(pixt, box, NULL);
    pixWrite(fileout, pixCrop, IFF_JFIF_JPEG); 
    

    
    return 0;

}

