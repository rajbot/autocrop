#!/usr/bin/python

"""
This script preprocesses images from an IA Scribe bookscanner.
For each image found in scandata.xml, it calculates a cropbox
and skew angle, and writes the results back into scandata.xml.
These can later be adjusted manually in Republisher.
"""

import argparse
import commands
import os
import re
import sys
import pipes
import math
#import xml.etree.ElementTree as ET
from lxml import etree as ET

if 3 != len(sys.argv):
    sys.exit('Usage: %s scandata.xml jpg_dir' % sys.argv[0])

AUTOCROP_VERSION = 0.1

# removeElements()
#______________________________________________________________________________
def removeElements(tag, parent):
    elements = parent.findall(tag)
    for element in elements:
        parent.remove(element)

# addCropBox()
#______________________________________________________________________________        
def addCropBox(tag, parent, x, y, w, h):
    removeElements(tag, parent)
    cropBox = ET.SubElement(parent, tag)
    ET.SubElement(cropBox, 'x').text = str(x)
    ET.SubElement(cropBox, 'y').text = str(y)
    ET.SubElement(cropBox, 'w').text = str(w)
    ET.SubElement(cropBox, 'h').text = str(h)

# get_autocrop_score()
# return a score between 0 and 5, depending on distance from mean
#______________________________________________________________________________        
def get_autocrop_score(length, mean, std_dev):
    diff = abs(mean - length)
    score = 5.0 - (diff/std_dev)
    if score < 0.0:
        score = 0.0
        
    return score

# addCropBoxAutoDetect()
#______________________________________________________________________________        
def addCropBoxAutoDetect(leaf, c, cropx, cropy, cropw, croph, width_mean_xo, height_mean_xo, width_std_dev_xo, height_std_dev_xo):
    removeElements('cropBoxAutoDetect', leaf)
    cropBox = ET.SubElement(leaf, 'cropBoxAutoDetect')    

    score_width  = get_autocrop_score(c['CleanCropR'] - c['CleanCropL'] + 1, width_mean_xo, width_std_dev_xo)
    score_height = get_autocrop_score(c['CleanCropB'] - c['CleanCropT'] + 1, height_mean_xo, height_std_dev_xo)
    score = score_width + score_height # score ranges from 0 to 10    
    ET.SubElement(cropBox, 'cropScore').text = "%0.2f"%score

    pass2Crop = ET.SubElement(cropBox, 'pass2Crop')
    ET.SubElement(pass2Crop, 'x').text = str(cropx)
    ET.SubElement(pass2Crop, 'y').text = str(cropy)
    ET.SubElement(pass2Crop, 'w').text = str(cropw)
    ET.SubElement(pass2Crop, 'h').text = str(croph)
    
    cleanCrop = ET.SubElement(cropBox, 'cleanCrop')
    ET.SubElement(cleanCrop, 'l').text = str(c['CleanCropL'])
    ET.SubElement(cleanCrop, 'r').text = str(c['CleanCropR'])
    ET.SubElement(cleanCrop, 't').text = str(c['CleanCropT'])
    ET.SubElement(cleanCrop, 'b').text = str(c['CleanCropB'])

    outerCrop = ET.SubElement(cropBox, 'outerCrop')
    ET.SubElement(outerCrop, 'l').text = str(c['OuterCropL'])
    ET.SubElement(outerCrop, 'r').text = str(c['OuterCropR'])
    ET.SubElement(outerCrop, 't').text = str(c['OuterCropT'])
    ET.SubElement(outerCrop, 'b').text = str(c['OuterCropB'])

    innerCrop = ET.SubElement(cropBox, 'innerCrop')
    ET.SubElement(innerCrop, 'l').text = str(c['InnerCropL'])
    ET.SubElement(innerCrop, 'r').text = str(c['InnerCropR'])
    ET.SubElement(innerCrop, 't').text = str(c['InnerCropT'])
    ET.SubElement(innerCrop, 'b').text = str(c['InnerCropB'])
    

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


# get_jpg()
#______________________________________________________________________________
def get_jpg(id, leafNum, jpg_dir):
    jpg = pipes.quote('%s/%s_orig_%04d.JPG' % (jpg_dir, id, leafNum))
    if not os.path.exists(jpg):
        jpg = pipes.quote('%s/%s_orig_%04d.jpg' % (jpg_dir, id, leafNum))
        
    assert os.path.exists(jpg)
    
    return jpg
    
# auto_crop_pass1()
#______________________________________________________________________________
def auto_crop_pass1(id, leafs, jpg_dir):
    crops = {}

    for leaf in leafs:
        leafNum = int(leaf.get('leafNum'))
        print 'Processing leaf %d, pass 1 ' % leafNum

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
        
        file = get_jpg(id, leafNum, jpg_dir)
        cmd = "./autoCropScribe %s %d" % (file, rotateDir)
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
        crops[leafNum]['angleConf'] = textScore
        crops[leafNum]['skewMode'] = skewMode
        crops[leafNum]['grayMode'] = grayMode

        #if (4==leafNum):
        #    break
            
    return crops


# auto_crop_pass2()
#______________________________________________________________________________
def auto_crop_pass2(leafs, crops):
    width_mean_full, width_var_full     = calc_mean_var(crops, 'CleanCropL', 'CleanCropR')
    height_mean_full, height_var_full   = calc_mean_var(crops, 'CleanCropT', 'CleanCropB')
    width_std_dev_full                  = math.sqrt(width_var_full)
    height_std_dev_full                 = math.sqrt(height_var_full)
    
    #recalculate, excluding outliers
    width_mean_xo, width_var_xo         = calc_mean_var_xo(crops, 'CleanCropL', 'CleanCropR', width_mean_full, width_std_dev_full)
    height_mean_xo, height_var_xo       = calc_mean_var_xo(crops, 'CleanCropT', 'CleanCropB', height_mean_full, height_std_dev_full)
    width_std_dev_xo                    = math.sqrt(width_var_xo)
    height_std_dev_xo                   = math.sqrt(height_var_xo)

    for leaf in leafs:
        leafNum = int(leaf.get('leafNum'))
        print 'Processing leaf %d, pass 2 ' % leafNum

        pageType = leaf.findtext('pageType')     
        if ('Delete' == pageType) or ('Color Card' == pageType) or ('White Card' == pageType) or ('Foldout' == pageType):
            print 'skipping pageType = ' + pageType
            continue #skip first deleted page

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
        
        addCropBox('cropBox', leaf, cropx, cropy, cropw, croph)
        addCropBoxAutoDetect(leaf, c, cropx, cropy, cropw, croph, width_mean_xo, height_mean_xo, width_std_dev_xo, height_std_dev_xo)

        removeElements('skewAngle', leaf)
        removeElements('skewAngleDetect', leaf)
        removeElements('skewScore', leaf)
        removeElements('skewActive', leaf)
        
        #set skewAngle to autoDetect angle, regardless of score
        ### TODO: the returned score is currently only the textSkew score, but sometimes we use the binding angle
        ET.SubElement(leaf, 'skewAngle').text = str(c['angle'])
        ET.SubElement(leaf, 'skewAngleDetect').text = str(c['angle'])
        ET.SubElement(leaf, 'skewScore').text = str(c['angleConf'])
        ET.SubElement(leaf, 'skewActive').text = 'true' #TODO: always set this to true, regardless of score.

        #if (4==leafNum):
        #    break

def argparser():
    """Parser for command line args"""
    parser = argparse.ArgumentParser(description="Autocrop calculates cropboxes " \
                                         "and skew angles for each image in " \
                                         "scandata.xml and wrides them to " \
                                         "scandata.xml")
    parser.add_argument('xml', help="scandata.xml file",
                        type=argparse.FileType('br+'))
    parser.add_argument('imgdir', help='image directory',
                        type=os.path.exists)
    return parser

def parsexml(xmlfp):
    """Returns the parsed lxml etree for a given xml file (fp or
    filename)
    """
    # remove_blank_text to enable pretty_printing later
    xmlparser = ET.XMLParser(remove_blank_text=True)
    xmltree = ET.parse(xmlfp, xmlparser)
    return xmltree

if __name__ == "__main__":
    parser = argparser()
    args = parser.parse_args()
    xmltree = parsexml(args.xml)

    scandata = xmltree.getroot()
    bookdata = scandata.find('bookData')
    bid = scandata.findtext('bookData/bookId')
    leafs = scandata.findall('.//page')
    
    print 'Autocropping ' + bid
    removeElements('autoCropVersion', bookdata)
    ET.SubElement(bookdata, 'autoCropVersion').text = str(AUTOCROP_VERSION)
    
    crops = auto_crop_pass1(bid, leafs, args.imgdir)
    auto_crop_pass2(leafs, crops)
    
    xmltree.write(args.xml, pretty_print=True)
