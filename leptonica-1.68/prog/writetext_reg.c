/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -  This software is distributed in the hope that it will be
 -  useful, but with NO WARRANTY OF ANY KIND.
 -  No author or distributor accepts responsibility to anyone for the
 -  consequences of using this software, or for whether it serves any
 -  particular purpose or works at all, unless he or she says so in
 -  writing.  Everyone is granted permission to copy, modify and
 -  redistribute this source code, for commercial or non-commercial
 -  purposes, with the following restrictions: (1) the origin of this
 -  source code must not be misrepresented; (2) modified versions must
 -  be plainly marked as such; and (3) this notice may not be removed
 -  or altered from any source or modified source distribution.
 *====================================================================*/

/*
 * writetext_reg.c
 *
 *   Regression test for writing a block of text in one of 4 locations
 *   relative to a pix.  This tests writing on 8 different types of images.
 *   Output is written to /tmp/pixd[1,2,3,4].png
 */

#include "allheaders.h"

void AddTextAndSave(PIXA *pixa, PIX *pixs, L_BMF *bmf, const char *textstr,
                    l_int32 location, l_uint32 val);

const char  *textstr[] =
           {"This is a simple test of text writing: 8 bpp",
            "This is a simple test of text writing: 32 bpp",
            "This is a simple test of text writing: 8 bpp cmapped",
            "This is a simple test of text writing: 4 bpp cmapped",
            "This is a simple test of text writing: 4 bpp",
            "This is a simple test of text writing: 2 bpp cmapped",
            "This is a simple test of text writing: 2 bpp",
            "This is a simple test of text writing: 1 bpp"};

const char  *topstr[] =
           {"Text is added above each image",
            "Text is added over the top of each image",
            "Text is added over the bottom of each image",
            "Text is added below each image"};

const l_uint32  colors[6] = {0x4090e000, 0x40e09000, 0x9040e000, 0x90e04000,
                             0xe0409000, 0xe0904000};


main(int    argc,
     char **argv)
{
char          outname[256], buf[512];
l_int32       loc, i;
L_BMF        *bmf, *bmftop;
PIX          *pixs, *pixt, *pixd;
PIX          *pix1, *pix2, *pix3, *pix4, *pix5, *pix6, *pix7, *pix8;
PIXA         *pixa;
L_REGPARAMS  *rp;
SARRAY       *sa;

    if (regTestSetup(argc, argv, &rp))
        return 1;

    bmf = bmfCreate("./fonts", 6);
    bmftop = bmfCreate("./fonts", 10);
    pixs = pixRead("lucasta-47.jpg");
    pix1 = pixScale(pixs, 0.4, 0.4);          /* 8 bpp grayscale */
    pix2 = pixConvertTo32(pix1);              /* 32 bpp rgb */
    pix3 = pixThresholdOn8bpp(pix1, 12, 1);   /* 8 bpp cmapped */
    pix4 = pixThresholdTo4bpp(pix1, 10, 1);   /* 4 bpp cmapped */
    pix5 = pixThresholdTo4bpp(pix1, 10, 0);   /* 4 bpp not cmapped */
    pix6 = pixThresholdTo2bpp(pix1, 3, 1);    /* 2 bpp cmapped */
    pix7 = pixThresholdTo2bpp(pix1, 3, 0);    /* 2 bpp not cmapped */
    pix8 = pixThresholdToBinary(pix1, 160);   /* 1 bpp */

    for (loc = 1; loc < 5; loc++) {
        pixa = pixaCreate(0);
        AddTextAndSave(pixa, pix1, bmf, textstr[0], loc, 800);
        AddTextAndSave(pixa, pix2, bmf, textstr[1], loc, 0xff000000);
        AddTextAndSave(pixa, pix3, bmf, textstr[2], loc, 0x00ff0000);
        AddTextAndSave(pixa, pix4, bmf, textstr[3], loc, 0x0000ff00);
        AddTextAndSave(pixa, pix5, bmf, textstr[4], loc, 800);
        AddTextAndSave(pixa, pix6, bmf, textstr[5], loc, 0xff000000);
        AddTextAndSave(pixa, pix7, bmf, textstr[6], loc, 800);
        AddTextAndSave(pixa, pix8, bmf, textstr[7], loc, 800);
        pixt = pixaDisplay(pixa, 0, 0);
        pixd = pixAddSingleTextblock(pixt, bmftop, topstr[loc - 1],
                                     0xff00ff00, L_ADD_ABOVE, NULL);
        regTestWritePixAndCheck(rp, pixd, IFF_PNG);  /*  0 - 4 */
        pixDisplayWithTitle(pixd, 50 * loc, 50, NULL, rp->display);
        pixDestroy(&pixt);
        pixDestroy(&pixd);
        pixaDestroy(&pixa);
    }

    pixDestroy(&pixs);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    pixDestroy(&pix5);
    pixDestroy(&pix6);
    pixDestroy(&pix7);
    pixDestroy(&pix8);
    bmfDestroy(&bmf);
    bmfDestroy(&bmftop);

        /* Write multiple lines in different colors, filling up
         * the colormap and requesting even more colors. */
    pixs = pixRead("weasel4.11c.png");
    pix1 = pixConvertTo8(pixs, 0);
    pix2 = pixScale(pixs, 8.0, 8.0);
    pix3 = pixQuantFromCmap(pix2, pixGetColormap(pixs), 4, 5,
                            L_EUCLIDEAN_DISTANCE);
    regTestWritePixAndCheck(rp, pix3, IFF_PNG);  /* 5 */
    pixDisplayWithTitle(pix3, 0, 500, NULL, rp->display);
    bmf = bmfCreate("fonts", 10);
    sa = sarrayCreate(6);
    for (i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "This is textline %d\n", i);
        sarrayAddString(sa, buf, L_COPY);
    }
    for (i = 0; i < 6; i++) {
        pixSetTextline(pix3, bmf, sarrayGetString(sa, i, L_NOCOPY),
                       colors[i], 50, 120 + 60 * i, NULL, NULL);
    }
    regTestWritePixAndCheck(rp, pix3, IFF_PNG);  /* 6 */
    pixDisplayWithTitle(pix3, 600, 500, NULL, rp->display);
    pixDestroy(&pixs);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    bmfDestroy(&bmf);
    sarrayDestroy(&sa);

    regTestCleanup(rp);
    return 0;
}


void
AddTextAndSave(PIXA        *pixa,
               PIX         *pixs,
               L_BMF       *bmf,
               const char  *textstr,
               l_int32      location,
               l_uint32     val)
{
l_int32  n, newrow, ovf;
PIX     *pixt;

    pixt = pixAddSingleTextblock(pixs, bmf, textstr, val, location, &ovf);
    n = pixaGetCount(pixa);
    newrow = (n % 4) ? 0 : 1;
    pixSaveTiledOutline(pixt, pixa, 1, newrow, 30, 2, 32);
    if (ovf) fprintf(stderr, "Overflow writing text in image %d\n", n + 1);
    pixDestroy(&pixt);
    return;
}

