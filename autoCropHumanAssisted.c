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
g++ -ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3  -Ileptonlib-1.56/src -I/usr/X11R6/include  -DL_LITTLE_ENDIAN -o autoCropHumanAssisted autoCropHumanAssisted.c autoCropCommon.c leptonlib-1.56/lib/nodebug/liblept.a -ltiff -ljpeg -lpng -lz -lm

to calculate cropbox:
autoCropHumanAssisted filein.jpg rotateDirection cropX cropY cropW cropH bindingGap
to calculate bindingGap:
autoCropHumanAssisted filein.jpg rotateDirection cropX cropY cropW cropH
*/

#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"
#include <assert.h>
#include <math.h>   //for sqrt
#include <float.h>  //for DBL_MAX
#include <limits.h> //for INT_MAX
#include "autoCropCommon.h"

#define debugstr printf
//#define debugstr


static const l_float32  deg2rad            = 3.1415926535 / 180.;




/// FindBindingGap()
///____________________________________________________________________________
void FindBindingGap(PIX       *pixg,
                    l_int32   rotDir,
                    l_float32 angle, 
                    l_int32   cropX,
                    l_int32   cropY,
                    l_int32   cropW, 
                    l_int32   cropH)
{               
    ///first, rotate image by angle
    PIX *pixt = pixRotate(pixg,
                    deg2rad*angle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );
    l_uint32 limitLeft = calcLimitLeft(w,h,angle);
    l_uint32 width10 = (l_uint32)(w * 0.10);

    l_uint32 left, right;
    if (1 == rotDir) {
        left  = limitLeft;
        right = width10;
    } else {
        left  = w - width10;
        right = w - limitLeft-1;
    }

    l_uint32   jTop = cropY;
    l_uint32   jBot = cropY + cropH;
    l_int32    bindingEdge;// = -1;
    l_uint32   bindingEdgeDiff;// = 0;
    float      bindingDelta = 0.0;
    CalculateSADcol(pixg, left, right, jTop, cropY+cropH, &bindingEdge, &bindingEdgeDiff);
    printf("strongest edge of gutter is at i=%d with diff=%d\n", bindingEdge, bindingEdgeDiff);
    
    double bindingLumaA = CalculateAvgCol(pixt, bindingEdge, jTop, jBot);
    printf("lumaA = %f\n", bindingLumaA);

    double bindingLumaB = CalculateAvgCol(pixt, bindingEdge+1, jTop, jBot);
    printf("lumaB = %f\n", bindingLumaB);

    double threshold = (l_uint32)((bindingLumaA + bindingLumaB) / 2);
    //TODO: ensure this threshold is reasonable
    printf("thesh = %f\n", threshold);
    
    //*thesh = (l_uint32)threshold;

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
        //return -1; //TODO: handle error
    }

    if (1 == rotDir) {  
        printf("bindingGap: %d\n", (cropX-rightEdge)*8);
    } else if (-1 == rotDir) {  
        printf("bindingGap: %d\n", (leftEdge-(cropX+cropW))*8);    
    } else {
        assert(0);
    }
    
    l_int32 darkThresh = CalculateTreshInitial(pixt);
    l_int32 topEdge    = RemoveBackgroundTop(pixt, rotDir, darkThresh);
    l_int32 bottomEdge = RemoveBackgroundBottom(pixt, rotDir, darkThresh);
    printf("topEdge: %d\n", topEdge);
    printf("bottomEdge: %d\n", bottomEdge);
    printf("topGap: %d\n", (cropY - topEdge) * 8);
    printf("bottomGap: %d\n", (bottomEdge - (cropY+cropH)) * 8);

    pixDestroy(&pixt);
}


/// SkewAndCrop()
///____________________________________________________________________________
void SkewAndCrop(PIX       *pixg,
                 l_int32   rotDir,
                 l_float32 oldAngle, 
                 l_int32   oldX,
                 l_int32   oldY,
                 l_int32   oldW, 
                 l_int32   oldH,
                 l_int32   gapBinding,
                 l_int32   gapTop,
                 l_int32   gapBottom,
                 PIX       *pixOrig)
{
printf("oldW = %d\n", oldW);
    l_int32 boxW10 = (l_int32)(oldW * 0.10);
    l_int32 boxH10 = (l_int32)(oldH * 0.10);
    BOX     *box   = boxCreate(oldX+boxW10, oldY+boxH10, oldW-boxW10, oldH-boxH10);
    PIX     *pixc  = pixClipRectangle(pixg, box, NULL);
    
    l_int32 darkThresh = CalculateTreshInitial(pixc);


    l_float32    conf, textAngle;

    PIX *pixSmall = pixScale(pixg, 0.125, 0.125);

    l_float32   bindingAngle;
    l_uint32    bindingThresh;
    
    l_int32 bindingEdge = FindBindingEdge2(pixSmall,
                                           rotDir,
                                           oldY/8,
                                           (oldY+oldH)/8,
                                           &bindingAngle,
                                           &bindingThresh, -1, -1);
    
    bindingEdge *= 8;

    PIX *pixb = pixThresholdToBinary(pixc, bindingThresh);
    //pixWrite("/tmp/home/rkumar/outbin.png", pixb, IFF_PNG); 

    printf("calling pixFindSkew\n");
    if (pixFindSkew(pixb, &textAngle, &conf)) {
        //error
        printf("angle=%.2f\nconf=%.2f\n", 0.0, -1.0);
    } else {
        printf("angle=%.2f\nconf=%.2f\n", textAngle, conf);
    }   
    

    #define   kSkewModeText 0
    #define   kSkewModeEdge 1
    l_int32   skewMode;
    l_float32 skewAngle;
    l_float32 skewConf;
    if (conf >= 2.0) {
        debugstr("skewMode: text\n");
        skewAngle = textAngle;
        skewMode  = kSkewModeText;
        skewConf  = conf;
    } else {

        debugstr("skewMode: edge\n");
        //angle = (deltaT + deltaB + deltaV1 + deltaV2)/4;
        skewAngle = bindingAngle; //TODO: calculate average of four edge deltas.
        skewMode  = kSkewModeEdge;
        skewConf  = 1.0;
    }

    printf("textAngle=%f\n", textAngle);
    printf("bindingAngle=%f\n", bindingAngle);
    printf("skewAngle=%.2f\n", skewAngle);
    printf("skewConf=%.2f\n", skewConf);


    //TODO: at this point, we need to rotate pixg, but it seems we didn't do that.

    l_int32 newX, newY, newW, newH;
    
    if (1 == rotDir) {
        newX = bindingEdge + gapBinding;
    } else if (-1 == rotDir) {
        printf("bindingEdge=%d, gapBinding=%d, oldW=%d\n", bindingEdge, gapBinding, oldW);
        newX = bindingEdge - gapBinding - oldW;    
    } else {
        assert(0);
    }

    l_int32 topEdge    = RemoveBackgroundTop(pixg, rotDir, bindingThresh);
    l_int32 bottomEdge = RemoveBackgroundBottom(pixg, rotDir, bindingThresh);
    
    printf("topEdge: %d\n", topEdge);
    printf("bottomEdge: %d\n", bottomEdge);

    //assert( (bottomEdge - topEdge) > oldH );
    float pageHeight = bottomEdge-topEdge;
    if (pageHeight > oldH) {
        newH = oldH;

        assert(gapTop>0);
        assert(gapBottom>0);
    
        l_int32 errorTop = abs(oldY - (topEdge+gapTop));
        l_int32 errorBottom = abs((oldY+oldH) - (bottomEdge - gapBottom));
    
        //newY = topEdge + ((bottomEdge-topEdge) - newH) / 2;
        if (errorTop<errorBottom) {
            printf("using top edge for crop box adjustment. errorTop = %d, errorBottom=%d\n", errorTop, errorBottom);
            newY = topEdge+gapTop;
        } else {
            printf("using bottom edge for crop box adjustment. errorTop = %d, errorBottom=%d\n", errorTop, errorBottom);
            newY = bottomEdge - gapBottom - oldH;
        }

    } else if (pageHeight/oldH > 0.90) {
        newH = bottomEdge-topEdge;
        newY = topEdge;
        printf("adjusting cropbox by %.2f%% to fit, newY = topEdge = %d\n", pageHeight/oldH, newY);
    } else {
        assert(0);
    }

    newW = oldW;


    
    printf("cropX=%d\n", newX);
    printf("cropY=%d\n", newY);
    printf("cropW=%d\n", newW);
    printf("cropH=%d\n", newH);
    if (1 == rotDir) {
        printf("bindingGap: %d\n", newX - bindingEdge);
    } else if (-1 == rotDir) {
        printf("bindingGap: %d\n", bindingEdge - (newX+newW));
    }
    printf("topGap: %d\n", newY - topEdge);
    printf("bottomGap: %d\n", bottomEdge - (newY+newH));

    #if 0   //for debugging
    PIX *pix = pixScale(pixOrig, 0.125, 0.125);
    pixWrite("/tmp/home/rkumar/out.jpg", pix, IFF_JFIF_JPEG); 
    
    PIX *pixr = pixRotate(pix,
                    deg2rad*skewAngle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);    
    BOX *boxOld = boxCreate(oldX/8, oldY/8, oldW/8, oldH/8);
    BOX *boxNew = boxCreate(newX/8, newY/8, newW/8, newH/8);

    pixRenderBoxArb(pixr, boxOld, 1, 255, 0, 0);
    pixRenderBoxArb(pixr, boxNew, 1, 0, 255, 0);
    pixWrite("/tmp/home/rkumar/outbox.jpg", pixr, IFF_JFIF_JPEG); 

    PIX *pix2 = pixScale(pixOrig, 0.125, 0.125);

    PIX *pixr2 = pixRotate(pix2,
                    deg2rad*skewAngle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    PIX *pixFinalC = pixClipRectangle(pixr2, boxNew, NULL);
    pixWrite("/tmp/home/rkumar/outcrop.jpg", pixFinalC, IFF_JFIF_JPEG); 
    #endif
}

/// AutoDeskewAndCrop()
///____________________________________________________________________________
void AutoDeskewAndCrop(PIX       *pixg,
                 l_int32   rotDir,
                 l_float32 oldAngle, 
                 l_int32   oldX,
                 l_int32   oldY,
                 l_int32   oldW, 
                 l_int32   oldH,
                 l_int32   oldMarginL,
                 l_int32   oldMarginR,
                 l_int32   oldMarginT,
                 l_int32   oldMarginB,
                 l_int32   oldThreshL,
                 l_int32   oldThreshR,
                 l_int32   oldThreshT,
                 l_int32   oldThreshB)
{

    l_int32 boxW10 = (l_int32)(oldW * 0.10);
    l_int32 boxH10 = (l_int32)(oldH * 0.10);
    BOX     *box   = boxCreate(oldX+boxW10, oldY+boxH10, oldW-boxW10, oldH-boxH10);
    PIX     *pixc  = pixClipRectangle(pixg, box, NULL);
    
    //l_int32 darkThresh = CalculateTreshInitial(pixc);

    //l_int32 maxThresh = max_int32(threshL, threshR);
    //maxThresh = max_int32(maxThresh, threshT);
    //maxThresh = max_int32(maxThresh, threshB);
    
    l_float32    conf, textAngle;

    PIX *pixSmall = pixScale(pixg, 0.125, 0.125);

    l_float32   bindingAngle;
    l_uint32    bindingThresh;
    

    l_int32 bindingEdge = FindBindingEdge2(pixSmall,
                                           rotDir,
                                           oldY/8+10,
                                           (oldY+oldH)/8,
                                           &bindingAngle,
                                           &bindingThresh,
                                           oldX/8+10,
                                           (oldX+oldW)/8);
    
    bindingEdge *= 8;
    DebugKeyValue_int32("bindingEdge", bindingEdge);        

    PIX *pixb = pixThresholdToBinary(pixc, bindingThresh);
    //pixWrite("/tmp/home/rkumar/outbin.png", pixb, IFF_PNG); 

    printf("calling pixFindSkew\n");
    if (pixFindSkew(pixb, &textAngle, &conf)) {
        //error
        //printf("angle=%.2f\nconf=%.2f\n", 0.0, -1.0);
    } else {
        //printf("angle=%.2f\nconf=%.2f\n", textAngle, conf);
    }   
    

    #define   kSkewModeText 0
    #define   kSkewModeEdge 1
    l_int32   skewMode;
    l_float32 skewAngle;
    l_float32 skewConf;
    if (conf >= 2.0) {
        PrintKeyValue_str("skewMode", "text");
        skewAngle = textAngle;
        skewMode  = kSkewModeText;
        skewConf  = conf;
    } else {

        PrintKeyValue_str("skewMode", "edge");
        //angle = (deltaT + deltaB + deltaV1 + deltaV2)/4;
        skewAngle = bindingAngle; //TODO: calculate average of four edge deltas.
        skewMode  = kSkewModeEdge;
        skewConf  = 1.0;
    }

    PrintKeyValue_float("textAngle", textAngle);
    PrintKeyValue_float("bindingAngle", bindingAngle);
    PrintKeyValue_float("skewAngle", skewAngle);
    PrintKeyValue_float("skewConf", skewConf);

    PIX *pixr = pixRotate(pixg,
                    deg2rad*skewAngle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    int foundPageL = 0;
    int foundPageR = 0;
    int foundPageT = 0;
    int foundPageB = 0;

    l_int32 pageL, pageR, pageT, pageB;
    l_int32 newX, newY, newH, newW;
    
    l_int32 limitL, limitR;
    ReduceRowOrCol(0.10, oldX, oldX+oldW-1, &limitL, &limitR);
    
    /// top
    l_int32 numBlackPels = CalculateNumBlackPelsRow(pixr, oldY, limitL, limitR, bindingThresh);
    printf("top num black pels=%d\n", numBlackPels);
    if (numBlackPels <= 10) {
        pageT = FindDarkRowUp(pixr, oldY, limitL, limitR, bindingThresh, 10);
        foundPageT = 1;
        DebugKeyValue_int32("newPageT", pageT);
    } else {
        //maybe we are in the text block. search up.
        l_int32 startY = FindWhiteRowUp(pixr, oldY, limitL, limitR, bindingThresh, 10);

        if (-1 != startY) {
            pageT = FindDarkRowUp(pixr, startY, limitL, limitR, bindingThresh, 10);
        } else {
            //not in text block. maybe we are above the page. search down
            pageT = FindWhiteRowDown(pixr, oldY, limitL, limitR, bindingThresh, 10);            
            assert(pageT);
        }        
        
        foundPageT = 1;
        DebugKeyValue_int32("newPageT", pageT);        
    }
    
    /// bottom
    /*
    l_int32 darkestPel = CalculateMinRow(pixr, oldY+oldH-1, oldX, oldX+oldW-1);
    l_int32 newThreshB;
    if ((darkestPel < oldThreshB) && (darkestPel > (oldThreshB*0.90))) {
        newThreshB = darkestPel;
    } else {
        newThreshB = oldThreshB;
    }
    */
    numBlackPels = CalculateNumBlackPelsRow(pixr, oldY+oldH-1, limitL, limitR, bindingThresh);
    printf("bottom num black pels=%d\n", numBlackPels);
    if (numBlackPels <= 10) {
        pageB = FindDarkRowDown(pixr, oldY+oldH-1, limitL, limitR, bindingThresh, 10);
        foundPageB = 1;
        DebugKeyValue_int32("newPageB", pageB);
    } else {
        //maybe we are in the text block. search down first.
        printf("finding white rows down\n");
        l_int32 downY = FindWhiteRowDown(pixr, oldY+oldH-1, limitL, limitR, bindingThresh, 10);
        printf("finding white rows up\n");
        l_int32 upY   = FindWhiteRowUp(pixr, oldY+oldH-1, limitL, limitR, bindingThresh, 10);
        DebugKeyValue_int32("downY", downY);
        DebugKeyValue_int32("upY", upY);
        if ( (-1 != upY) && (-1 != downY)) {
            l_int32 oldCropB = oldX+oldW-1;
            DebugKeyValue_int32("oldCropB", oldCropB);
            if (abs(upY-oldCropB) < abs(downY - oldCropB)) {
                pageB = upY;
            } else {
                pageB = downY;
            }
            foundPageB = 1;
            DebugKeyValue_int32("newPageB", pageB);
        } else if (-1 != upY) {
            pageB = upY;
            foundPageB = 1;
            DebugKeyValue_int32("newPageB", pageB);
        } else if (-1 != downY) {
            pageB = downY;
            foundPageB = 1;
            DebugKeyValue_int32("newPageB", pageB);
        } else {
            assert(0);
        }
    }

    /// left side
    l_int32 limitT, limitB;
    ReduceRowOrCol(0.10, oldY, oldY+oldH-1, &limitT, &limitB);
    numBlackPels = CalculateNumBlackPelsCol(pixr, oldX, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh /*oldThreshL*/);
    printf("left num black pels=%d\n", numBlackPels);
    if (numBlackPels <= 10) {
        pageL = FindDarkColLeft(pixr, oldX, limitT, limitB, bindingThresh, 10);
        foundPageL = 1;
        DebugKeyValue_int32("newPageL", pageL);
    } else {
        if (-1 == rotDir) {
            //left hand leaf, binding on right side
            pageL = FindWhiteColLeft(pixr,  oldX, limitT, limitB, bindingThresh, 10);
            if (-1 == pageL) {
                pageL = FindWhiteColRight(pixr,  oldX, limitT, limitB, bindingThresh, 10);
                assert(-1 != pageL);
            }
            foundPageL = 1;            
            DebugKeyValue_int32("newPageL", pageL);
        } else if (1 == rotDir) {
            //right hand leaf, binding on left side
            pageL = FindWhiteColRight(pixr, bindingEdge, limitT, limitB, bindingThresh, 10);
            assert(-1 != pageL);
            foundPageL = 1;
            DebugKeyValue_int32("newPageL", pageL);  
        } else {
            assert(0);
        }
    }

    /// right side
    numBlackPels = CalculateNumBlackPelsCol(pixr, oldX+oldW-1, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh);
    printf("right num black pels=%d\n", numBlackPels);
    if (numBlackPels <= 10) {
        pageR = FindDarkColRight(pixr, oldX+oldW-1, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh, 10);
        foundPageR = 1;
        DebugKeyValue_int32("newPageR", pageR);
    } else {
        if (-1 == rotDir) {
            //left hand leaf, binding on right side
            printf("binding edge at %d\n", bindingEdge);
            //if (bindingEdge > (oldX+oldW-1)) {
                pageR = FindWhiteColLeft(pixr, bindingEdge /*oldX+oldW-1*/, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh, 10);
                assert(-1 != pageR);
                foundPageR = 1;
                DebugKeyValue_int32("newPageR", pageR);
            //}
        } else if (1 == rotDir) {
            //right hand leaf, binding on left side
            pageR = FindWhiteColRight(pixr,  oldX+oldW-1, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh, 10);
            if (-1 == pageR) {
                pageR = FindWhiteColLeft(pixr,  oldX+oldW-1, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh, 10);
                assert(-1 != pageR);
            }
            
            foundPageR = 1;
            DebugKeyValue_int32("newPageR", pageR);
            
        } else {
            assert(0);
        }
    }
    

    
    assert(foundPageL & foundPageR & foundPageT & foundPageB);
    
    /// Now, find the text block bounderies
    l_int32 h = pixGetHeight(pixr);
    l_int32 w = pixGetWidth(pixr);
    

    //TODO: use newThresh[l,r,t,b] here instead of bindingThresh
    l_int32 textBlockL = FindDarkColRight(pixr, pageL+1, limitT /*oldY*/, limitB /*oldY+oldH-1*/, bindingThresh, 10);
    if (-1 == textBlockL) textBlockL = min_int32(oldX+((l_int32)(w*0.10)), w);
    DebugKeyValue_int32("textBlockL", textBlockL);

    l_int32 textBlockR = FindDarkColLeft(pixr, pageR-1 /*oldX+oldW-1*/, limitT, limitB, bindingThresh, 10);
    if (-1 == textBlockR) textBlockR = max_int32(oldX+oldW-1-((l_int32)(w*0.10)), 0);
    DebugKeyValue_int32("textBlockR", textBlockR);

    l_int32 textBlockT = FindDarkRowDown(pixr, pageT+1 /*oldY*/, limitL, limitR, bindingThresh, 10);
    if (-1 == textBlockT) textBlockT = min_int32(oldY+((l_int32)(h*0.10)), h);
    DebugKeyValue_int32("textBlockT", textBlockT);

    l_int32 textBlockB = FindDarkRowUp(pixr, pageB-1 /*oldY+oldH-1*/, limitL, limitR, bindingThresh, 10);
    if (-1 == textBlockB) textBlockB = max_int32(oldY+oldH-1-((l_int32)(h*0.10)), 0);
    DebugKeyValue_int32("textBlockB", textBlockB);
    
    assert(textBlockL>pageL);
    assert(textBlockR<pageR);
    assert(textBlockT>pageT);
    assert(textBlockB<pageB);

    /// Place crop box horizontally
    l_int32 textBlockW = textBlockR-textBlockL;
    l_int32 pageW = pageR-pageL;
    if (1 == rotDir) {
        //right-hand leaf
        //binding on left side
        if ((oldW < pageW) && (oldW > textBlockW)) {

            if (((pageL+oldMarginL)<textBlockL) && ((pageL+oldMarginL+oldW)>textBlockR) && ((pageL+oldMarginL+oldW)<pageR) ) {
                debugstr("cropBox width perfect fit!\n");
                newX = pageL + oldMarginL;
                newW = oldW;
            } else if ((pageL+oldMarginL)<textBlockL) {
                newX = pageL+oldMarginL;
                newW = pageR-newX;
                printf("REDUCING cropWidth by %.2f percent!\n", 100.0*((float)(oldW-newW))/((float)oldW) );                
            } else {
                printf("pageL+oldMarginL = %d \t textBlockL = %d\n", pageL+oldMarginL , textBlockL);
                printf("pageL+oldMarginL+oldW = %d \t textBlockR = %d\n", pageL+oldMarginL+oldW, textBlockR);
                printf("pageL+oldMarginL+oldW = %d \t pageR = %d\n", pageL+oldMarginL+oldW, pageR);            
                assert(0);
            }

        } else if (oldW>textBlockW) {
            int tmpR;
            if ((pageL+oldMarginL) < textBlockL) {
                newX = pageL+oldMarginL;
            } else {
                newX = pageL;
            }
            if ((pageR-oldMarginR) > textBlockR) {
                tmpR = pageR-oldMarginR; 
            } else {
                tmpR = pageR;
            }
            newW = tmpR - newX;
            printf("REDUCING cropWidth by %.2f percent!\n", 100.0*((float)(oldW-newW))/((float)oldW) );            
        } else {
        
            assert(0);
        }
    } else if (-1 == rotDir) {
        if ((oldW < (pageR-pageL)) && (oldW > (textBlockR-textBlockL))) {
            if (((pageR-oldMarginR)>textBlockR) && ((pageR-oldMarginR-oldW)<textBlockL) && ((pageR-oldMarginR-oldW)>pageL) ) {
                debugstr("cropBox width perfect fit!\n");
                newX = pageR - oldMarginR - oldW;
                newW = oldW;
            } else if ((pageR-oldMarginR)>textBlockR) {
                newX = pageL;
                newW = pageR-oldMarginR-pageL;
                printf("REDUCING cropWidth by %.2f percent!\n", 100.0*((float)(oldW-newW))/((float)oldW) );
            } else if ( ((pageL+oldMarginL) > pageL) && ((pageL+oldMarginL) < textBlockL) ) {
                newX = pageL+oldMarginL;
                newW = pageR-newX;
                printf("REDUCING cropWidth by %.2f percent!\n", 100.0*((float)(oldW-newW))/((float)oldW) );
            } else {
                printf("pageR-oldMarginR = %d \t textBlockR = %d\n", pageR-oldMarginR , textBlockR);
                printf("pageR-oldMarginR-oldW = %d \t textBlockL = %d\n", pageR-oldMarginR-oldW, textBlockL);
                printf("pageR-oldMarginR-oldW = %d \t pageL = %d\n", pageR-oldMarginR-oldW, pageL);            
                assert(0);
            }
        } else if (oldW>textBlockW) {
            int tmpR;
            if ((pageL+oldMarginL) < textBlockL) {
                newX = pageL+oldMarginL;
            } else {
                newX = pageL;
            }
            if ((pageR-oldMarginR) > textBlockR) {
                tmpR = pageR-oldMarginR; 
            } else {
                tmpR = pageR;
            }
            newW = tmpR - newX;
            printf("REDUCING cropWidth by %.2f percent!\n", 100.0*((float)(oldW-newW))/((float)oldW) );
        } else {
            assert(0);
        }
    } else {
        assert(0);
    }

    /// Place crop box vertically
    //  favor bottom edge, because top edge often has bookmarks sticking out :(
    if ( ((pageB-oldMarginB)>textBlockB) && ((pageB-oldMarginB-oldH)<textBlockT) && ((pageB-oldMarginB-oldH)>pageT) ) {
        debugstr("cropBox height perfect fit! using bottom edge\n");
        newY = pageB - oldMarginB - oldH;
        newH = oldH;    
    } else if ( ((pageT+oldMarginT)<textBlockT) && ((pageT+oldMarginT+oldH)>textBlockB) && ((pageT+oldMarginT+oldH)<pageB) ) { 
        debugstr("cropBox height perfect fit! using top edge\n");
        newY = pageT + oldMarginT;
        newH = oldH;
    } else if ( (oldH < (pageB-pageT)) && (oldH > (textBlockB-textBlockT)) ) {
        debugstr("centering cropBox height\n");
        newY = pageT + (( (pageB-pageT) - oldH) / 2);
        newH = oldH;
    } else if ( (oldH >= (pageB-pageT)) && (oldH > (textBlockB-textBlockT)) ) {
        newH = pageB-pageT;
        newY = pageT;
        printf("REDUCING cropHeight by %.2f percent!\n", 100.0*((float)(oldH-newH))/((float)oldH) );

    } else {
        printf("pageB-oldMarginB = %d \t textBlockB = %d\n", pageB-oldMarginB, textBlockB);
        printf("pageB-oldMarginB-oldH = %d \t textBlockT = %d\n", pageB-oldMarginB-oldH, textBlockT);
        printf("pageB-oldMarginB-oldH = %d \t pageT = %d\n", pageB-oldMarginB-oldH, pageT);        
        
        printf("pageT+oldMarginT = %d \t textBlockT = %d\n", pageT+oldMarginT, textBlockT);
        printf("pageT+oldMarginT+oldH = %d \t textBlockB = %d\n", pageT+oldMarginT+oldH, textBlockB);
        printf("pageT+oldMarginT+oldH = %d \t pageB = %d\n", pageT+oldMarginT+oldH, pageB);
        assert(0);
    }

    //TODO: update these with values calculated from this page    
    PrintKeyValue_int32("threshL", oldThreshL);
    PrintKeyValue_int32("threshR", oldThreshR);
    PrintKeyValue_int32("threshT", oldThreshT);
    PrintKeyValue_int32("threshB", oldThreshB);
    

    PrintKeyValue_int32("marginL", newX-pageL);
    PrintKeyValue_int32("marginR", pageR-(newX+newW));
    PrintKeyValue_int32("marginT", newY-pageT);
    PrintKeyValue_int32("marginB", pageB - (newY+newH));

    PrintKeyValue_int32("cropX", newX);
    PrintKeyValue_int32("cropY", newY);
    PrintKeyValue_int32("cropW", newW);
    PrintKeyValue_int32("cropH", newH);

    #if 0   //for debugging
    PIX *pix = pixScale(pixOrig, 0.125, 0.125);
    pixWrite("/tmp/home/rkumar/out.jpg", pix, IFF_JFIF_JPEG); 
    
    PIX *pixr = pixRotate(pix,
                    deg2rad*skewAngle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);    
    BOX *boxOld = boxCreate(oldX/8, oldY/8, oldW/8, oldH/8);
    BOX *boxNew = boxCreate(newX/8, newY/8, newW/8, newH/8);

    pixRenderBoxArb(pixr, boxOld, 1, 255, 0, 0);
    pixRenderBoxArb(pixr, boxNew, 1, 0, 255, 0);
    pixWrite("/tmp/home/rkumar/outbox.jpg", pixr, IFF_JFIF_JPEG); 

    PIX *pix2 = pixScale(pixOrig, 0.125, 0.125);

    PIX *pixr2 = pixRotate(pix2,
                    deg2rad*skewAngle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    PIX *pixFinalC = pixClipRectangle(pixr2, boxNew, NULL);
    pixWrite("/tmp/home/rkumar/outcrop.jpg", pixFinalC, IFF_JFIF_JPEG); 
    #endif
}

/// FindPageMargins()
///____________________________________________________________________________
void FindPageMargins(PIX       *pixg,
                   l_int32   rotDir,
                   l_float32 angle, 
                   l_int32   cropX,
                   l_int32   cropY,
                   l_int32   cropW, 
                   l_int32   cropH)
{
    ///first, rotate image by angle
    PIX *pixt = pixRotate(pixg,
                    deg2rad*angle,
                    L_ROTATE_AREA_MAP,
                    L_BRING_IN_BLACK,0,0);

    BOX     *box   = boxCreate(cropX, cropY, cropW, cropH);
    PIX     *pixc  = pixClipRectangle(pixt, box, NULL);
    l_int32 blackThresh = CalculateTreshInitial(pixc);

    l_uint32 w = pixGetWidth( pixg );
    l_uint32 h = pixGetHeight( pixg );

    PIX *pixSmall = pixScale(pixg, 0.125, 0.125);
    l_float32   bindingAngle;
    l_uint32    bindingThresh;   
    l_int32 bindingEdge = FindBindingEdge2(pixSmall,
                                           rotDir,
                                           cropY/8,
                                           (cropY+cropH)/8,
                                           &bindingAngle,
                                           &bindingThresh, -1, -1);    
    bindingEdge *= 8;

    /// Top
    l_uint32 darkestPelTop = CalculateMinRow(pixt, cropY, cropX, cropX+cropW-1);
    //assert(darkestPelTop > (l_uint32((l_float32)blackThresh*0.90)));
    l_int32 pageTop = FindDarkRowUp(pixt, cropY, cropX, cropX+cropW-1, bindingThresh /*darkestPelTop*/, 10);
    assert(-1 != pageTop);

    /// Bottom
    l_uint32 darkestPelBottom = CalculateMinRow(pixt, cropY+cropH-1, cropX, cropX+cropW-1);
    //assert(darkestPelBottom > (l_uint32((l_float32)blackThresh*0.90)));
    l_int32 pageBottom = FindDarkRowDown(pixt, cropY+cropH-1, cropX, cropX+cropW-1, bindingThresh /*darkestPelBottom*/, 10);
    assert(-1 != pageBottom);

    /// Left
    l_uint32 darkestPelLeft = CalculateMinCol(pixt, cropX, cropY, cropY+cropH-1);
    //assert(darkestPelLeft > (l_uint32((l_float32)blackThresh*0.90)));
    l_int32 pageLeft = FindDarkColLeft(pixt, cropX, cropY, cropY+cropH-1, bindingThresh /*darkestPelLeft*/, 10);

    /// Right
    l_uint32 darkestPelRight = CalculateMinCol(pixt, cropX+cropW-1, cropY, cropY+cropH-1);
    //assert(darkestPelRight > (l_uint32((l_float32)blackThresh*0.90)));
    l_int32 pageRight = FindDarkColRight(pixt, cropX+cropW-1, cropY, cropY+cropH-1, bindingThresh /*darkestPelRight*/, 10);

    PrintKeyValue_int32("bindingThresh", bindingThresh);

    PrintKeyValue_int32("threshL", darkestPelLeft);
    PrintKeyValue_int32("threshR", darkestPelRight);
    PrintKeyValue_int32("threshT", darkestPelTop);
    PrintKeyValue_int32("threshB", darkestPelBottom);
    
    PrintKeyValue_int32("pageL", pageLeft);
    PrintKeyValue_int32("pageR", pageRight);
    PrintKeyValue_int32("pageT", pageTop);
    PrintKeyValue_int32("pageB", pageBottom);

    PrintKeyValue_int32("marginL", cropX-pageLeft);
    PrintKeyValue_int32("marginR", pageRight-(cropX+cropW-1));
    PrintKeyValue_int32("marginT", cropY-pageTop);
    PrintKeyValue_int32("marginB", pageBottom - (cropY+cropH-1));
    
    //pixWrite("gray.png", pixt, IFF_PNG); 

}

/// main()
///____________________________________________________________________________
int main(int argc, char **argv) {
    static char  mainName[] = "autoCropHumanAssisted";

    if ((argc != 8) && (argc != 16)) {
        exit(ERROR_INT(" Syntax:  autoCropHumanAssist filein.jpg rotateDirection angle cropX cropY cropW cropH [marginL marginR marginT marginB threshL threshR threshT threshB]",
                         mainName, 1));
    }
    
    char    *filein  = argv[1];
    l_int32   rotDir   = atoi(argv[2]);
    l_float32 angle    = atof(argv[3]); //this is usually ignored; only used in findBindingEdge mode
    l_int32   cropX    = atoi(argv[4]);
    l_int32   cropY    = atoi(argv[5]);
    l_int32   cropW    = atoi(argv[6]);
    l_int32   cropH    = atoi(argv[7]);    
    
    FILE        *fp;
    PIX         *pixs, *pixd, *pixg;
    
    /*
    if ((fp = fopenReadStream(filein)) == NULL) {
        exit(ERROR_INT("image file not found", mainName, 1));
    }
    debugstr("Opened file handle\n");

    if (8 == argc) {
        if ((pixs = pixReadStreamJpeg(fp, 0, 8, NULL, 0)) == NULL) {
           exit(ERROR_INT("pixs not made", mainName, 1));
        }
    } else {
        startTimer();
        if ((pixs = pixReadStreamJpeg(fp, 0, 1, NULL, 0)) == NULL) {
           exit(ERROR_INT("pixs not made", mainName, 1));
        }
        printf("opened large jpg in %7.3f sec\n", stopTimer());    
    }
    debugstr("Read jpeg\n");
    */
    if (8 == argc) {
        /*
        PIX *pixtmp;  
        if ((pixtmp = pixRead(filein)) == NULL) {
        exit(ERROR_INT("pixtmp not made", mainName, 1));
        }
        pixs = pixScale(pixtmp, 0.125, 0.125);
        */
        if ((pixs = pixRead(filein)) == NULL) {
        exit(ERROR_INT("pixs not made", mainName, 1));
        }
    } else {
        if ((pixs = pixRead(filein)) == NULL) {
        exit(ERROR_INT("pixs not made", mainName, 1));
        }
    }

    if (rotDir) {
        pixd = pixRotate90(pixs, rotDir);
        debugstr("Rotated 90 degrees\n");
    } else {
        pixd = pixs;
    }
    
    pixg = ConvertToGray(pixd);
    debugstr("Converted to gray\n");
    //pixWrite("/tmp/home/rkumar/outgray.jpg", pixg, IFF_JFIF_JPEG); 
    
    if (8 == argc) {
        //FindBindingGap(pixg, rotDir, angle, cropX/8, cropY/8, cropW/8, cropH/8);
        FindPageMargins(pixg, rotDir, angle, cropX, cropY, cropW, cropH);
    } else {
        /*
        l_int32 gapBinding = atoi(argv[8]);
        l_int32 gapTop     = atoi(argv[9]);
        l_int32 gapBottom  = atoi(argv[10]);
        SkewAndCrop(pixg, rotDir, angle, cropX, cropY, cropW, cropH, gapBinding, gapTop, gapBottom, pixd);
        */
        l_int32 marginL = atoi(argv[8]);
        l_int32 marginR = atoi(argv[9]);
        l_int32 marginT = atoi(argv[10]);
        l_int32 marginB = atoi(argv[11]);

        l_int32 threshL = atoi(argv[12]);
        l_int32 threshR = atoi(argv[13]);
        l_int32 threshT = atoi(argv[14]);
        l_int32 threshB = atoi(argv[15]);

        AutoDeskewAndCrop(pixg, rotDir, angle, cropX, cropY, cropW, cropH, 
                          marginL, marginR, marginT, marginB,
                          threshL, threshR, threshT, threshB);

    }

    
    pixDestroy(&pixs);
    pixDestroy(&pixd);
    pixDestroy(&pixg);
    
    return 0;
}
