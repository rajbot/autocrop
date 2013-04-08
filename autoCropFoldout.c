/*
Copyright(c)2008-2013 Internet Archive. Software license GPL version 2.

run with:
autoCropFoldout filein.jpg
*/

#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"
#include <assert.h>
#include <math.h>   //for sqrt
#include <float.h>  //for DBL_MAX
#include <limits.h> //for INT_MAX
#include "autoCropCommon.h"
#include "autocrop_remove_bg.h"


#define debugstr printf
//#define debugstr
//#define WRITE_DEBUG_IMAGES 1

static const l_float32  deg2rad            = 3.1415926535 / 180.;


static inline l_int32 min (l_int32 a, l_int32 b) {
    return b + ((a-b) & (a-b)>>31);
}

static inline l_int32 max (l_int32 a, l_int32 b) {
    return a - ((a-b) & (a-b)>>31);
}

/// main()
///____________________________________________________________________________
int main(int argc, char **argv) {
    PIX         *pixs, *pixd, *pixg;
    char        *filein;
    static char  mainName[] = "autoCropFoldout";
    FILE        *fp;

    if (argc != 2) {
        exit(ERROR_INT(" Syntax:  autoCropFoldout filein.jpg",
                         mainName, 1));
    }

    filein  = argv[1];

    if ((fp = fopenReadStream(filein)) == NULL) {
        exit(ERROR_INT("image file not found", mainName, 1));
    }
    debugstr("Opened file handle\n");

    if ((pixs = pixReadStreamJpeg(fp, 0, 8, NULL, 0)) == NULL) {
       exit(ERROR_INT("pixs not made", mainName, 1));
    }
    debugstr("Read jpeg\n");

    //we don't rotate foldouts
    pixd = pixs;

    #ifdef WRITE_DEBUG_IMAGES
    pixWrite(DEBUG_IMAGE_DIR "out.jpg", pixd, IFF_JFIF_JPEG);
    #endif

    l_int32 grayChannel;
    pixg = ConvertToGray(pixd, &grayChannel);
    debugstr("Converted to gray\n");

    l_int32 histmax;
    l_int32 threshInitial = CalculateTreshInitial(pixg, &histmax);
    debugstr("threshInitial is %d\n", threshInitial);


    float delta;


    l_int32 cropT=-1, cropB=-1, cropR=-1, cropL=-1;
    float deltaT, deltaB, deltaV1, deltaV2, deltaBinding, deltaOuter;
    l_uint32 threshOuter, threshT, threshB;


    /* create bitonal image */
    PIX *pixb;
    l_int32    w_8, h_8, d;
    pixGetDimensions(pixg, &w_8, &h_8, &d);

    pixOtsuAdaptiveThreshold(pixg,
                             w_8,
                             h_8,
                             1,
                             1,
                             0.1,
                             NULL,
                             &pixb);

    #ifdef WRITE_DEBUG_IMAGES
    {
        pixWrite(DEBUG_IMAGE_DIR "outbininit.png", pixb, IFF_PNG);
    }
    #endif


    //Perform quick removal of background
    l_int32 topEdge = remove_bg_top(pixb, 0);
    debugstr("topEdge is %d\n", topEdge);

    l_int32 bottomEdge = remove_bg_bottom(pixb, 0);
    debugstr("bottomEdge is %d\n", bottomEdge);

    assert(bottomEdge>topEdge);

    l_int32 rightEdge = remove_bg_outer(pixb, 1,  topEdge, bottomEdge);
    l_int32 leftEdge  = remove_bg_outer(pixb, -1, topEdge, bottomEdge);
    debugstr("rightEdge is %d\n", rightEdge);
    debugstr("leftEdge is %d\n", leftEdge);

    threshInitial = 158;

    BOX *box;
    l_int32 boxW10 = (l_int32)(0.10*(rightEdge-leftEdge)*8);
    l_int32 boxH10 = (l_int32)(0.10*(bottomEdge-topEdge)*8);

    box     = boxCreate(leftEdge*8+boxW10, topEdge*8+boxH10, (rightEdge-leftEdge)*8-2*boxW10, (bottomEdge-topEdge)*8-2*boxH10);

    double skewScore, skewConf;

    PIX *pixBig;

    startTimer();
    if ((pixBig = pixRead(filein)) == NULL) {
       exit(ERROR_INT("pixBig not made", mainName, 1));
    }
    printf("opened large jpg in %7.3f sec\n", stopTimer());

    PIX *pixBigG;
    if (kGrayModeThreeChannel != grayChannel) {
        pixBigG = pixConvertRGBToGray (pixBig, (0==grayChannel), (1==grayChannel), (2==grayChannel));
    } else {
        pixBigG = pixConvertRGBToGray (pixBig, 0.30, 0.60, 0.10);
    }



    PIX *pixBigBFull; // = pixThresholdToBinary (pixBigC, threshInitial);
    pixOtsuAdaptiveThreshold(pixBigG,
                             w_8,
                             h_8,
                             50,
                             50,
                             0.1,
                             NULL,
                             &pixBigBFull);

    PIX *pixBigB = pixClipRectangle(pixBigBFull, box, NULL);
    debugstr("croppedWidth = %d, croppedHeight=%d\n", pixGetWidth(pixBigB), pixGetHeight(pixBigB));

    #ifdef WRITE_DEBUG_IMAGES
    {
        pixWrite(DEBUG_IMAGE_DIR "outbinfull.png", pixBigBFull, IFF_PNG);
        pixWrite(DEBUG_IMAGE_DIR "outbinbig.png", pixBigB, IFF_PNG);
    }
    #endif


    l_float32    angle, conf, textAngle;

    debugstr("calling pixFindSkew\n");
    if (pixFindSkew(pixBigB, &textAngle, &conf)) {
      /* an error occured! */
        debugstr("textAngle=%.2f\ntextConf=%.2f\n", 0.0, -1.0);
     } else {
        debugstr("textAngle=%.2f\ntextConf=%.2f\n", textAngle, conf);
    }


    //Deskew(pixbBig, cropL*8, cropR*8, cropT*8, cropB*8, &skewScore, &skewConf);

    l_int32 skewMode;

    if (conf >= 2.0) {
        printf("skewMode: text\n");
        angle = textAngle;
        skewMode = kSkewModeText;
    } else {
        printf("skewMode: none\n");
        angle = 0.0;
        skewMode = kSkewModeNone;
    }

    debugstr("rotating by %f\n", angle);

    PIX *pixBigT = pixRotate(pixBigG,
                    deg2rad*angle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    cropT = topEdge*8;
    cropB = bottomEdge*8;
    cropL = leftEdge*8;
    cropR = rightEdge*8;

    l_int32 outerCropL = cropL;
    l_int32 outerCropR = cropR;
    l_int32 outerCropT = cropT;
    l_int32 outerCropB = cropB;



    debugstr("finding clean lines...\n");

    l_int32 w = pixGetWidth(pixBigT);
    l_int32 h = pixGetHeight(pixBigT);

    l_int32 limitLeft = calcLimitLeft(w,h,angle);
    l_int32 limitTop  = calcLimitTop(w,h,angle);

    PIX *pixBigTbin;
    pixOtsuAdaptiveThreshold(pixBigT,
                             w/2,
                             h/2,
                             50,
                             50,
                             0.1,
                             NULL,
                             &pixBigTbin);

    #ifdef WRITE_DEBUG_IMAGES
    {
        pixWrite(DEBUG_IMAGE_DIR "outbinT.png", pixBigTbin, IFF_PNG);
    }
    #endif


    //redo the rough crop on the full-resolution rotated image
    cropT = remove_bg_top(pixBigTbin, 0);
    debugstr("new cropT is %d\n", cropT);

    cropB = remove_bg_bottom(pixBigTbin, 0);
    debugstr("bottomEdge is %d\n", cropB);

    assert(bottomEdge>topEdge);

    cropR = remove_bg_outer(pixBigTbin, 1,  cropT, cropB);
    cropL = remove_bg_outer(pixBigTbin, -1, cropT, cropB);
    debugstr("rightEdge is %d\n", rightEdge);
    debugstr("leftEdge is %d\n", leftEdge);


    debugstr("adjusted: cL=%d, cR=%d, cT=%d, cB=%d limitLeft=%d, limitTop=%d\n", cropL, cropR, cropT, cropB, limitLeft, limitTop);

    printf("angle: %.2f\n", angle);
    printf("conf: %.2f\n", conf); //TODO: this is the text deskew angle, but what if we are deskewing using the binding mode?
    PrintKeyValue_int32("OuterCropL", cropL);
    PrintKeyValue_int32("OuterCropR", cropR);
    PrintKeyValue_int32("OuterCropT", cropT);
    PrintKeyValue_int32("OuterCropB", cropB);

    //debugstr("finding inner crop box (text block)...\n");
    //l_int32 innerCropT, innerCropB, innerCropL, innerCropR;
    //FindInnerCrop(pixBigT, threshInitial, cropL, cropR, cropT, cropB, &innerCropL, &innerCropR, &innerCropT, &innerCropB);

    #ifdef WRITE_DEBUG_IMAGES
    {
        //BOX *boxCrop = boxCreate(cropL/8, cropT/8, (cropR-cropL)/8, (cropB-cropT)/8);

        PIX *p = pixCopy(NULL, pixd);

        PIX *pixFinalR = pixRotate(p,
                        deg2rad*angle,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);


        //pixRenderBoxArb(pixFinalR, boxCrop, 1, 255, 0, 0);

        Box *boxOuterCrop = boxCreate(cropL/8, cropT/8, (cropR-cropL)/8, (cropB-cropT)/8);
        pixRenderBoxArb(pixFinalR, boxOuterCrop, 1, 0, 0, 255);

        /*
        if ((-1 != innerCropL) & (-1 != innerCropR) & (-1 != innerCropT) & (-1 != innerCropB)) {
            BOX *boxCropVar = boxCreate(innerCropL/8, innerCropT/8, (innerCropR-innerCropL)/8, (innerCropB-innerCropT)/8);
            pixRenderBoxArb(pixFinalR, boxCropVar, 1, 0, 255, 0);
            boxDestroy(&boxCropVar);
        }
        */

        pixWrite(DEBUG_IMAGE_DIR "outbox.jpg", pixFinalR, IFF_JFIF_JPEG);
        pixDestroy(&pixFinalR);
        pixDestroy(&p);

        p = pixCopy(NULL, pixd);
        PIX *pixFinalR2 = pixRotate(p,
                        deg2rad*angle,
                        L_ROTATE_AREA_MAP,
                        L_BRING_IN_BLACK,0,0);

        PIX *pixFinalC = pixClipRectangle(pixFinalR2, boxOuterCrop, NULL);
        pixWrite(DEBUG_IMAGE_DIR "outcrop.jpg", pixFinalC, IFF_JFIF_JPEG);
        pixDestroy(&p);
        pixDestroy(&pixFinalC);

        //boxDestroy(&boxCrop);
        boxDestroy(&boxOuterCrop);

    }
    #endif

}
