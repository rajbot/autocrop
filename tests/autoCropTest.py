#!/usr/bin/python

"""
This script autocrops jpg images in the current directory.
"""

import commands
import glob
import os
import re
import math
import datetime
import xml.etree.ElementTree as ET

AUTOCROP_VERSION = 0.1

files = glob.glob("*.jpg") + glob.glob("*.JPG")
assert len(files) > 0

#outDir      = '/var/www/autocrop/' + os.path.basename(os.getcwd()).split('_')[0]
outDir      = '/home/rkumar/public_html/autocrop/' + os.path.basename(os.getcwd()).split('_')[0]

proxyDir   = 'proxy'
skewedDir  = 'skewed'
croppedDir = 'cropped'
humanDir   = 'human'
pass2Dir   = 'pass2'

assert not os.path.exists(outDir)
assert not os.path.exists(outDir + '/' + proxyDir)
assert not os.path.exists(outDir + '/' + skewedDir)
assert not os.path.exists(outDir + '/' + croppedDir)
assert not os.path.exists(outDir + '/' + humanDir)
assert not os.path.exists(outDir + '/' + pass2Dir)
assert os.path.exists('scandata.xml');

os.mkdir(outDir)
os.mkdir(outDir + '/' + proxyDir)
os.mkdir(outDir + '/' + skewedDir)
os.mkdir(outDir + '/' + croppedDir)
os.mkdir(outDir + '/' + humanDir)
os.mkdir(outDir + '/' + pass2Dir)

outHtml = outDir + '/skew.html'
assert not os.path.exists(outHtml)

# parse_int()
#_______________________________________________________________________________
def parse_int(key, output, d):
    m=re.search(key + ': (-?\d+)', output)
    assert(None != m)
    val = int(m.group(1))
    print "%s = %d" % (key, val) 
    d[key] = val
    return val


# calc_mean_var()
#_______________________________________________________________________________
def calc_mean_var(crops, keyA, keyB):
    mean = 0.0;
    num_leafs = 0;
    for leafnum,c in crops.items():
        w = float(c[keyB] - c[keyA] + 1)
        mean += w
        num_leafs+=1

    mean /= num_leafs
    
    var = 0;

    for leafnum,c in crops.items():
        w = float(c[keyB] - c[keyA] + 1)
        var += ( (w-mean)*(w-mean) )

    var /= num_leafs        
    
    print 'mean = %f' % mean
    print 'var = %f' % var
    return mean, var     

# calc_mean_var_xo()
# exclude outliers that are more than one std dev from mean, and
# recalculate mean/var
#_______________________________________________________________________________
def calc_mean_var_xo(crops, keyA, keyB, mean_full, std_dev_full):
    mean_xo = 0.0;
    num_leafs = 0;
    for leafnum,c in crops.items():
        w = float(c[keyB] - c[keyA] + 1)
        print "w = %f, mean_full=%f, std_dev_full=%f abs_diff=%f" % (w, mean_full, std_dev_full, abs(w-mean_full)),
        if abs(w-mean_full) <= std_dev_full:
            mean_xo += w
            num_leafs+=1
            print "accepting"
        else:
            print "rejecting"

    mean_xo /= num_leafs
    
    var_xo = 0;

    for leafnum,c in crops.items():
        w = float(c[keyB] - c[keyA] + 1)
        if abs(w-mean_full) <= std_dev_full:
            var_xo += ( (w-mean_xo)*(w-mean_xo) )

    var_xo /= num_leafs
    
    print 'mean_xo = %f' % mean_xo
    print 'var_xo = %f' % var_xo
    return mean_xo, var_xo


# fit_crop_width_simple()
#_______________________________________________________________________________
def fit_crop_width_simple(c, mean_width, width_std_dev, img_width, hand_side):
    clean_width = c['CleanCropR'] - c['CleanCropL'] + 1
    #inner_width = c['InnerCropR'] - c['InnerCropL'] + 1 #WATCH OUT: InnerCropR/L may be -1
    outer_width = c['OuterCropR'] - c['OuterCropL'] + 1
    clean_width_D2 = clean_width*0.5
    outer_width_D2 = outer_width*0.5
    mean_width_D2  = mean_width*0.5
    
    clean_mid = c['CleanCropL'] + clean_width_D2
    outer_mid = c['OuterCropL'] + outer_width_D2
    
    normalize_width = False
    
    if abs(clean_width - mean_width) > width_std_dev:
        print "clean_width greater than one std dev from mean, using clean crop"
        cropx = c['CleanCropL']
        cropw = clean_width
    else:
        print "clean_width within one std dev from mean, attempting to use mean_width to crop this page"
        ### cropx = int(clean_mid - (mean_width*0.5))
        ### the binding is usually cropped tightly, so set one side of the crop box to the clean binding crop
        cropw = int(mean_width)
        if ('RIGHT' == hand_side):
            cropx = c['CleanCropL']  #binding on left
        else:
            cropx = c['CleanCropR'] - cropw + 1 #binding on right
        normalize_width = True
        
        if cropx < 0:
            cropx = 0
        elif (cropx + cropw - 1) >= img_width: #W = R-L+1, R = W+L-1
            #we want to set R = img_width - 1
            cropx = img_width - cropw
        
    return cropx, cropw, normalize_width    


# fit_crop_height_simple()
#_______________________________________________________________________________
def fit_crop_height_simple(c, mean_height, height_std_dev, img_height):
    clean_height = c['CleanCropB'] - c['CleanCropT'] + 1
    #inner_height = c['InnerCropB'] - c['InnerCropT'] + 1 #WATCH OUT: InnerCropR/L may be -1
    outer_height = c['OuterCropB'] - c['OuterCropT'] + 1
    clean_height_D2 = clean_height*0.5
    outer_height_D2 = outer_height*0.5
    mean_height_D2  = mean_height*0.5
    
    clean_mid = c['CleanCropT'] + clean_height_D2
    outer_mid = c['OuterCropT'] + outer_height_D2
    
    normalize_height = False
    
    if abs(clean_height - mean_height) > height_std_dev:
        print "clean_height greater than one std dev from mean, using clean crop"
        cropy = c['CleanCropT']
        croph = clean_height
    else:
        if (mean_height <= clean_height):
            print "clean_height within one std dev from mean, attempting to use mean_height to crop this page"
            cropy = int(clean_mid - (mean_height_D2)) #center inside clean crop box
            croph = int(mean_height)
            normalize_height = True
            
            if cropy < 0:
                cropy = 0
            elif (cropy + croph - 1) >= img_height: #H = T-B+1, B = H+T-1
                #we want to set B = img_width - 1
                cropy = img_height - croph
        else:
            print "mean_height > clean_height, using clean_height"
            cropy = c['CleanCropT']
            croph = clean_height
            
    return cropy, croph, normalize_height
    
# fit_crop_width()
# TODO: This function is not yet finished!
#_______________________________________________________________________________
def fit_crop_width(c, mean_width, width_std_dev):
    clean_width = c['CleanCropR'] - c['CleanCropL'] + 1
    #inner_width = c['InnerCropR'] - c['InnerCropL'] + 1 #WATCH OUT: InnerCropR/L may be -1
    outer_width = c['OuterCropR'] - c['OuterCropL'] + 1
    clean_width_D2 = clean_width*0.5
    outer_width_D2 = outer_width*0.5
    mean_width_D2  = mean_width*0.5
    
    clean_mid = c['CleanCropL'] + clean_width_D2
    outer_mid = c['OuterCropL'] + outer_width_D2
    
    if abs(clean_width - mean_width) > width_std_dev:
        print "clean_width greater than one std dev from mean, using clean crop"
        cropx = c['CleanCropL']
        cropw = clean_width
    else:
        print "clean_width within one std dev from mean, attempting to use mean_width to crop this page"
        
        if (mean_width >= outer_width):
            print "  oops. mean_width greater than outer_width.. using outer width"
            cropx = c['OuterCropL']
            cropw = outer_width
        elif (mean_width >= clean_width):
            print "  mean_width greater than clean_width but smaller than outer width"
            
            if ((clean_mid - mean_width_D2) >= c['OuterCropL']) and ((clean_mid + mean_width_D2) <= c['OuterCropR']):
                print "    centering mean_width around CleanCrop"
                cropx = int(clean_mid - (mean_width*0.5))
                cropw = int(mean_width)
            else:
                print "    centering width inside OuterCrop" #TODO: what if this crop intersects clean crop?
                cropx = int(outer_mid - (mean_width*0.5))
                cropw = mean_width
        #elif (mean_width >= inner_width):
        #    print "  mean_width greater than inner_width but smaller than clean_width"
        #    print "    centering mean_width inside CleanCrop" #TODO: what if this crop intersects inner crop?
        #    cropx = int(clean_mid - (width_mean*0.5))
        #    cropw = int(width_mean)
        else:
            print "  mean_width smaller than clean_width"
            print "   centering mean_width around CleanCrop" #TODO: if inner crop box was found, use it to center mean_width
            cropx = int(clean_mid - (mean_width*0.5))
            cropw = int(mean_width)

    return cropx, cropw            

# __main__
#_______________________________________________________________________________

scandata = ET.parse('scandata.xml').getroot()
leafs = scandata.findall('.//page')
id = scandata.findtext('bookData/bookId')

crops = {}

#pass 1
for file in sorted(files):
    m = re.search('(\d{4})\.(jpg|JPG)$', file)
    leafNum = int(m.group(1))
    print 'leafNum is %d' % leafNum

    print "Processing %s, leafNum=%d, "%(file, leafNum),

    leaf=leafs[leafNum]

    pageType = leaf.findtext('pageType') 
    if ('Delete' == pageType) or ('Color Card' == pageType) or ('White Card' == pageType) or ('Foldout' == pageType):
        print 'skipping pageType = ' + pageType
        continue #skip first deleted page


    rotateDegree = int(leaf.findtext('rotateDegree'))
    assert((-90 == rotateDegree) or (90 == rotateDegree))    
    if (90 == rotateDegree):
        rotateDir = 1
    elif (-90 == rotateDegree):
        rotateDir = -1

    print "rotateDir = %d" % rotateDir
        
    cmd = "/home/rkumar/gnubook/autoCropScribe %s %d" % (file, rotateDir)
    print cmd
    retval,output = commands.getstatusoutput(cmd)
    if 0 != retval:
        print "retval is %d" % (retval)
        print output
        
    assert (0 == retval)

    m=re.search('skewMode: (\w+)', output)
    skewMode = m.group(1)
    print "skewMode is " + skewMode
    
    m=re.search('angle: ([-.\d]+)', output)
    assert(None != m)
    textSkew = float(m.group(1))

    m=re.search('conf: ([-.\d]+)', output)
    assert(None != m)
    textScore = float(m.group(1))
    
    m=re.search('bindingAngle: ([-.\d]+)', output)
    assert(None != m)
    bindingSkew = float(m.group(1))

    m=re.search('grayMode: ([\w-]+)', output)
    grayMode = m.group(1)
    print "grayMode is " + grayMode

    crops[leafNum] = {}
    parse_int('OuterCropL', output, crops[leafNum] )
    parse_int('OuterCropR', output, crops[leafNum] )
    parse_int('OuterCropT', output, crops[leafNum] )
    parse_int('OuterCropB', output, crops[leafNum] )
    parse_int('CleanCropL', output, crops[leafNum] )
    parse_int('CleanCropR', output, crops[leafNum] )
    parse_int('CleanCropT', output, crops[leafNum] )
    parse_int('CleanCropB', output, crops[leafNum] )
    parse_int('InnerCropL', output, crops[leafNum] )
    parse_int('InnerCropR', output, crops[leafNum] )
    parse_int('InnerCropT', output, crops[leafNum] )
    parse_int('InnerCropB', output, crops[leafNum] )
    crops[leafNum]['angle'] = textSkew
    crops[leafNum]['skewMode'] = skewMode
    crops[leafNum]['grayMode'] = grayMode
        
    retval = commands.getstatusoutput('cp /home/rkumar/public_html/out.jpg "%s/%s/%d.jpg"'%(outDir, proxyDir,leafNum))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp /home/rkumar/public_html/outbox.jpg "%s/%s/%d.jpg"'%(outDir, skewedDir,leafNum))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp /home/rkumar/public_html/outcrop.jpg "%s/%s/%d.jpg"'%(outDir, croppedDir,leafNum))[0]
    assert (0 == retval)    
    
    ### don't compare to human crop/skew    
    # skew = float(leaf.findtext('skewAngle'))
    # #score = float(leaf.findtext('skewScore'))
    # skewAct = leaf.findtext('skewActive')
    # cropx = int(leaf.findtext('cropBox/x'))
    # cropy = int(leaf.findtext('cropBox/y'))
    # cropw = int(leaf.findtext('cropBox/w'))
    # croph = int(leaf.findtext('cropBox/h'))
    # if "false" == skewAct:
    #     skew = 0.0


    ### don't compare to human crop/skew    
    # retval = commands.getstatusoutput('/home/rkumar/gnubook/tests/cropAndSkewProxy %s "%s/%s/%d.jpg" %d %f %d %d %d %d'%(file, outDir, humanDir,leafNum, rotateDir, skew, cropx/8, cropy/8, cropw/8, croph/8))[0]
    # assert (0 == retval)    
    
    #if (4==leafNum):
    #    break
    


width_mean, width_var = calc_mean_var(crops, 'CleanCropL', 'CleanCropR')
height_mean, height_var = calc_mean_var(crops, 'CleanCropT', 'CleanCropB')
width_std_dev = math.sqrt(width_var)
height_std_dev = math.sqrt(height_var)

#recalculate, excluding outliers
width_mean_xo, width_var_xo = calc_mean_var_xo(crops, 'CleanCropL', 'CleanCropR', width_mean, width_std_dev)
height_mean_xo, height_var_xo = calc_mean_var_xo(crops, 'CleanCropT', 'CleanCropB', height_mean, height_std_dev)
width_std_dev_xo = math.sqrt(width_var_xo)
height_std_dev_xo = math.sqrt(height_var_xo)


#pass 2
f = open(outHtml, 'w')
f.writelines(['<html>\n', '<head><title>autocrop test for %s</title></head>\n'%id, '<body>\n', 'autocrop test for <a href="http://www.archive.org/details/%s">%s</a><br>\n'%(id, id), 'autocrop version = %f, run on %s\n'%(AUTOCROP_VERSION, datetime.datetime.now().isoformat())]);

f.write('<table border=2>\n')
f.write('<tr>')
f.write('<th>Original Image</th>')
f.write('<th>Pass 1 Autocrop and deskew results</th>')
f.write('<th>Pass 1 Autocropped image</th>')
f.write('<th>Pass 2 Autocropped image</th>')
f.write('</tr>\n')

for file in sorted(files):
    m = re.search('(\d{4})\.(jpg|JPG)$', file)
    leafNum = int(m.group(1))
    print 'leafNum is %d' % leafNum

    print "Processing %s, leafNum=%d, "%(file, leafNum),

    leaf=leafs[leafNum]

    pageType = leaf.findtext('pageType') 
    if ('Delete' == pageType) or ('Color Card' == pageType) or ('White Card' == pageType) or ('Foldout' == pageType):
        print 'skipping pageType = ' + pageType
        continue #skip first deleted page

    rotateDegree = int(leaf.findtext('rotateDegree'))
    assert((-90 == rotateDegree) or (90 == rotateDegree))    
    if (90 == rotateDegree):
        rotateDir = 1
    elif (-90 == rotateDegree):
        rotateDir = -1

    print "rotateDir = %d" % rotateDir
        
    c = crops[leafNum]
    cropx = c['CleanCropL']
    cropw = c['CleanCropR'] - c['CleanCropL'] + 1
    
    cropy = c['CleanCropT']
    croph = c['CleanCropB'] - c['CleanCropT'] + 1

    img_width  = int(leaf.findtext('origWidth'))
    img_height = int(leaf.findtext('origHeight'))
    
    print "Pass 1 crop x,y = (%d, %d) w,h = (%d, %d)" % (cropx, cropy, cropw, croph)
    
    hand_side = leaf.findtext('handSide') 
    cropx, cropw, normalize_width  = fit_crop_width_simple(c, width_mean_xo, width_std_dev_xo, img_width, hand_side)
    cropy, croph, normalize_height = fit_crop_height_simple(c, height_mean_xo, height_std_dev_xo, img_height)
    print "Pass 2 crop x,y = (%d, %d) w,h = (%d, %d)" % (cropx, cropy, cropw, croph)
    
    ### don't compare to human crop/skew    
    # retval = commands.getstatusoutput('/home/rkumar/gnubook/tests/cropAndSkewProxy %s "%s/%s/%d.jpg" %d %f %d %d %d %d'%(file, outDir, humanDir,leafNum, rotateDir, skew, cropx/8, cropy/8, cropw/8, croph/8))[0]
    # assert (0 == retval)    

    cmd = '/home/rkumar/gnubook/tests/cropAndSkewProxy %s "%s/%s/%d.jpg" %d %f %d %d %d %d'%(file, outDir, pass2Dir,leafNum, rotateDir, c['angle'], cropx/8, cropy/8, cropw/8, croph/8)
    print cmd
    retval = commands.getstatusoutput(cmd)[0]
    assert (0 == retval)    
    

    f.write('<tr>\n')
    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(proxyDir,leafNum))
    f.write("<br>leaf %d"%(leafNum))
    f.write('</td>\n')

    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(skewedDir,leafNum))
    f.write("<br>skewMode: %s"%(c['skewMode']))
    f.write("<br>angle: %0.2f"%(c['angle']))
    #f.write("<br>bindingSkew: %0.2f"%(bindingSkew))
    f.write("<br>grayMode: %s"%(c['grayMode']))
    f.write('</td>\n')


    f.write('<td valign=top>'),
    f.write("pass1 crop and skew:<br>")
    f.write('<img src="%s/%d.jpg"/>'%(croppedDir,leafNum))
    f.write("<br>CleanCrop x,y = (%d, %d)" % (c['CleanCropL'], c['CleanCropT']))
    f.write("<br>CleanCrop w,h = (%d, %d)" % (c['CleanCropR'] - c['CleanCropL'] + 1, c['CleanCropB'] - c['CleanCropT'] + 1))
    f.write('</td>\n')

    ### don't compare to human crop/skew
    # f.write('<td valign=top>'),
    # f.write("human crop and skew:<br>")
    # f.write('<img src="%s/%d.jpg"/>'%(humanDir,leafNum))
    # f.write("<br>Skew: %0.2f, crop x,y w,h = %d,%d %d,%d"%(skew, cropx, cropy, cropw, croph))
    # f.write('</td>\n')

    f.write('<td valign=top>'),
    f.write("pass2 crop and skew:<br>")
    f.write('<img src="%s/%d.jpg"/>'%(pass2Dir,leafNum))
    f.write("<br>Pass 2 x,y = (%d,%d)"%(cropx, cropy))
    f.write("<br>Pass 2 w,h = (%d,%d)"%(cropw, croph))
    if normalize_width:
        f.write("<br>NORMALIZED WIDTH");
    if normalize_height:
        f.write("<br>NORMALIZED HEIGHT");
    f.write('</td>\n')

    f.write('</tr>\n')
    f.flush()
    
        
    
    #if (4==leafNum):
    #    break
    
f.writelines(['</table>\n', '</html>\n'])
f.close()