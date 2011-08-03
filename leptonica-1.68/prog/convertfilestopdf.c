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
 * convertfilestopdf.c
 *
 *    Converts all image files in the given directory with matching substring
 *    to a pdf, with the specified scaling factor <= 1.0 applied to all
 *    images.
 *
 *    See below for syntax and usage.
 *
 *    The images are displayed at a resolution that depends on the
 *    input resolution (res) and the scaling factor (scalefact) that
 *    is applied to the images before conversion to pdf.  Internally
 *    we multiply these, so that the generated pdf will render at the
 *    same resolution as if it hadn't been scaled.  By downscaling, you
 *    reduce the size of the images.  For jpeg, downscaling reduces
 *    pdf size by the square of the scale factor.  It also regenerates
 *    the jpeg with quality = 75.
 */

#include <string.h>
#include "allheaders.h"

main(int    argc,
     char **argv)
{
char        *dirin, *substr, *title, *fileout;
l_int32      ret, res;
l_float32    scalefactor;
static char  mainName[] = "convertfilestopdf";

    if (argc != 7) {
        fprintf(stderr,
            " Syntax: convertfilestopdf dirin substr res"
            " scalefactor title fileout\n"
            "         dirin:  input directory for image files\n"
            "         substr:  Use 'allfiles' to convert all files\n"
            "                  in the directory.\n"
            "         res:  Input resolution of each image;\n"
            "               assumed to all be the same\n"
            "         scalefactor:  Use to scale all images\n"
            "         title:  Use 'none' to omit\n"
            "         fileout:  Output pdf file\n");
        return 1;
    }

    dirin = argv[1];
    substr = argv[2];
    res = atoi(argv[3]);
    scalefactor = atof(argv[4]);
    title = argv[5];
    fileout = argv[6];
    if (!strcmp(substr, "allfiles"))
        substr = NULL;
    if (scalefactor <= 0.0 || scalefactor > 1.0) {
        L_WARNING("invalid scalefactor: setting to 1.0", mainName);
        scalefactor = 1.0;
    }
    if (!strcmp(title, "none"))
        title = NULL;

    ret = convertFilesToPdf(dirin, substr, res, scalefactor,
                            75, title, fileout);
    return ret;
}

