l_uint32 calcLimitLeft(l_uint32 w, l_uint32 h, l_float32 angle);

l_int32 FindBindingEdge2(PIX      *pixg,
                         l_int32  rotDir,
                         l_uint32 topEdge,
                         l_uint32 bottomEdge,
                         float    *skew,
                         l_uint32 *thesh);

PIX* ConvertToGray(PIX *pix);


double CalculateAvgCol(PIX      *pixg,
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

l_int32 CalculateTreshInitial(PIX *pixg);

l_int32 RemoveBackgroundTop(PIX *pixg, l_int32 rotDir, l_int32 initialBlackThresh);
l_int32 RemoveBackgroundBottom(PIX *pixg, l_int32 rotDir, l_int32 initialBlackThresh);
