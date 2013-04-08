#ifndef AUTOCROP_AUTOCROPCOMMON_H
#define AUTOCROP_AUTOCROPCOMMON_H

#define DEBUG_IMAGE_DIR "./debug-images/"

#define debugstr printf
//#define debugstr

//Binary PIX structs store black as 1
#define PEL_IS_BLACK 1
#define PEL_IS_WHITE 0

#define kGrayModeSingleChannel 1
#define kGrayModeThreeChannel  3

l_uint32 calcLimitLeft(l_uint32 w, l_uint32 h, l_float32 angle);
l_uint32 calcLimitTop(l_uint32 w, l_uint32 h, l_float32 angle);

l_int32 FindBindingEdge2(PIX      *pixg,
                         l_int32  rotDir,
                         l_uint32 topEdge,
                         l_uint32 bottomEdge,
                         float    *skew,
                         l_uint32 *thesh,
                         l_int32 textBlockL,
                         l_int32 textBlockR);

PIX* ConvertToGray(PIX *pix, l_int32 *grayChannel);


double CalculateAvgCol(PIX      *pixg,
                       l_uint32 i,
                       l_uint32 jTop,
                       l_uint32 jBot);

double CalculateAvgRow(PIX      *pixg,
                       l_uint32 j,
                       l_uint32 iLeft,
                       l_uint32 iRight);

double CalculateVarRow(PIX      *pixg,
                       l_uint32 j,
                       l_uint32 iLeft,
                       l_uint32 iRight);

double CalculateVarCol(PIX      *pixg,
                       l_uint32 i,
                       l_uint32 jTop,
                       l_uint32 jBot);

l_uint32 CalculateSADcol(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   jTop,
                         l_uint32   jBot,
                         l_int32    *reti,
                         l_uint32   *retDiff
                        );

l_uint32 CalculateSADrow(PIX        *pixg,
                         l_uint32   left,
                         l_uint32   right,
                         l_uint32   top,
                         l_uint32   bottom,
                         l_int32    *reti,
                         l_uint32   *retDiff
                        );

l_int32 CalculateTreshInitial(PIX *pixg, l_int32 *histmax);

l_int32 RemoveBackgroundTop(PIX *pixg, l_int32 rotDir, l_int32 initialBlackThresh);
l_int32 RemoveBackgroundBottom(PIX *pixg, l_int32 rotDir, l_int32 initialBlackThresh);

l_int32 CalculateNumBlackPelsRow(PIX *pixg, l_int32 j, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh);
l_int32 CalculateNumBlackPelsCol(PIX *pixg, l_int32 i, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh);
l_int32 CalculateMinRow(PIX *pixg, l_int32 j, l_int32 limitL, l_int32 limitR);
l_int32 CalculateMinCol(PIX *pixg, l_int32 i, l_int32 limitT, l_int32 limitB);

l_int32 FindDarkRowUp(PIX *pixg, l_int32 limitB, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindDarkRowDown(PIX *pixg, l_int32 limitT, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindDarkColLeft(PIX *pixg, l_int32 limitR, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindDarkColRight(PIX *pixg, l_int32 limitL, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindWhiteRowUp(PIX *pixg, l_int32 limitB, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindWhiteRowDown(PIX *pixg, l_int32 limitB, l_int32 limitL, l_int32 limitR, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindWhiteColLeft(PIX *pixg, l_int32 limitR, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit);
l_int32 FindWhiteColRight(PIX *pixg, l_int32 limitL, l_int32 limitT, l_int32 limitB, l_uint32 blackThresh, l_int32 blackLimit);

void PrintKeyValue_int32(const char *key, l_int32 val);
void DebugKeyValue_int32(const char *key, l_int32 val);
void PrintKeyValue_float(const char *key, l_float32 val);
void PrintKeyValue_str(const char *key, char *val);

l_int32 min_int32(l_int32 a, l_int32 b);
l_int32 max_int32(l_int32 a, l_int32 b);

void ReduceRowOrCol(l_float32 percent, l_int32 oldT, l_int32 oldB, l_int32 *newT, l_int32 *newB);

l_int32 FindBindingUsingBlackBar(PIX      *pixg,
                                 l_int32  rotDir,
                                 l_int32 topEdge,
                                 l_int32 bottomEdge,
                                 l_int32 textBlockL,
                                 l_int32 textBlockR);


l_int32 RemoveBackgroundOuter(PIX *pixg, l_int32 rotDir, l_uint32 topEdge, l_uint32 bottomEdge, l_int32 initialBlackThresh);

l_uint32 RemoveBlackPelsBlockColRight(PIX *pixg, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom, l_uint32 kernelWidth, l_uint32 blackThresh);
l_uint32 RemoveBlackPelsBlockColLeft(PIX *pixg, l_uint32 starti, l_uint32 endi, l_uint32 top, l_uint32 bottom, l_uint32 kernelWidth, l_uint32 blackThresh);
l_uint32 RemoveBlackPelsBlockRowTop(PIX *pixg, l_uint32 startj, l_uint32 endj, l_uint32 left, l_uint32 right, l_uint32 kernelWidth, l_uint32 blackThresh);
l_uint32 RemoveBlackPelsBlockRowBot(PIX *pixg, l_uint32 startj, l_uint32 endj, l_uint32 left, l_uint32 right, l_uint32 kernelWidth, l_uint32 blackThresh);

int FindInnerCrop(PIX *pixBigT,
    l_uint32 threshBinding,
    l_int32 outerCropL,
    l_int32 outerCropR,
    l_int32 outerCropT,
    l_int32 outerCropB,
    l_int32 *innerCropL,
    l_int32 *innerCropR,
    l_int32 *innerCropT,
    l_int32 *innerCropB);

#endif
