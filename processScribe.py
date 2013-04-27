#-*- coding: utf-8 -*-
#!/usr/bin/python

"""
    processScribe
    ~~~~~~~~~~~~~

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
from lxml import etree as ET

__version__ = 0.1

BOOK_DATA = 'bookData'
BOOK_ID = 'bookData/bookId'
PAGES = './/page'
PAGE_BLACKLIST = ['Delete', 'Color Card', 'White Card', 'Foldout']

def rmtag(tag, parent):
    """Remove all occurrences of a tag from its parent

    usage:
    >>> from StringIO import StringIO
    >>> from lxml import etree
    >>> xml = "<parent><child>1</child><child>2</child></parent>"
    >>> xmltree = etree.parse(StringIO(xml))
    >>> xmltree.findall("child")
    [<Element child at 0x2ba9550>, <Element child at 0x2ba95a0>]
    >>> rmtag("child", xmltree.getroot())
    >>> xmltree.findall("child")
    []
    """
    for child in parent.findall(tag):
        parent.remove(child)

def addCropBox(tag, parent, x, y, w, h):
    """Clears/Removes all pre-existing instances of tag from within
    the parent and replaces them with the the xml markup designating
    the location and dimentions of cropbox

    params:
    :param x, y: (x, y) coordinates where to add the cropbox
    :param w, h: width and height to make the cropbox
    """
    rmtag(tag, parent)
    cropBox = ET.SubElement(parent, tag)
    for k, v in zip(('x', 'y', 'w', 'h'), (x, y, w, h)):
        ET.SubElement(cropBox, k).text = str(v)

def crop_score(length, mean, std_dev):
    """calculates a score between [0, 5] inclusive, depending on the
    length's distance from mean

    usage
    >>> crop_score(4, 2, 1)
    3.0
    """
    diff = abs(mean - length)
    score = 5.0 - (diff / std_dev)
    return 0.0 if score < 0.0 else score

def mean_var(crops, key1, key2, std_dev=None):
    """Calculates the mean and variance of two crop types within a dict of crops

    params:

    :param std_dev: a standard deviation. If a value is provided for
                    stddev other than the value None, mean_var will
                    return a second mean and variance which exclude
                    outliers that are more than one std dev from mean.
    usage: 
    >>> mean_var(crops, 'CleanCropL', 'CleanCropR')
    12.3, .89
    >>> mean_var(crops, 'CleanCropL', 'CleanCropR', std_dev=7)
    (12.3, 6.21), (.89, .22)
    """
    values = [float(crop[key2] - crop[key1] + 1) for i, crop in enumerate(crops)]
    samplesize = len(values)
    total = sum(values)
    mean = total / samplesize
    variance = sum(((value - mean) ** 2) for value in values) / samplesize

    if std_dev is not None:
        mean2, variance2 = None, None
        #return (mean, mean2), (variance, variance2)
        raise NotImplementedError
    return mean, variance

def addCropBoxAutoDetect(leaf, c, cropx, cropy, cropw, croph,
                         width_mean_xo, height_mean_xo,
                         width_std_dev_xo, height_std_dev_xo):
    """
    usage:
    >>> from StringIO import StringIO
    >>> from lxml import etree
    >>> xml = "<parent><child>1</child><child>2</child></parent>"
    >>> xmltree = etree.parse(StringIO(xml))
    >>> leaf = xmltree.getroot()
    >>>
    """

    def init_cropbox():
        """Appends a new xml tag called 'cropBoxAutoDetect' to the
        xmltree, calculates the accumulative crop_score for the crop,
        c, and appends this value as field 'cropScore' under its
        parent 'cropBoxAutoDetect'
        """
        rmtag('cropBoxAutoDetect', leaf)
        cropBox = ET.SubElement(leaf, 'cropBoxAutoDetect')
        crop_width = c['CleanCropR'] - c['CleanCropL'] + 1
        crop_height = c['CleanCropB'] - c['CleanCropT'] + 1
        score_width = crop_score(crop_width, width_mean_xo, width_std_dev_xo)
        score_height = crop_score(crop_height, height_mean_xo, height_std_dev_xo)
        score = score_width + score_height # score ranges from 0 to 10
        ET.SubElement(cropBox, 'cropScore').text = "%0.2f" % score
        return cropBox

    cropbox = init_cropbox()
    pass2Crop = ET.SubElement(cropbox, 'pass2Crop')
    for k, v in zip(('x', 'y', 'w', 'h'), (cropx, cropy, cropw, croph)):
        ET.SubElement(pass2Crop, k).text = str(v)

    for crop in ('clean', 'outer', 'inner'):
        # Iterate over borders: top, right, bottom, left
        for border in ('t', 'r', 'b', 'l'): 
            cropkey = '%sCrop%s' % (crop.capitalize(), border.capitalize())
            croptype = ET.SubElement(cropbox, "%sCrop" % crop)
            ET.SubElement(croptype, border).text = str(c[cropkey])    

def parse_int(key, output, d):
    m = re.search(key + ': (-?\d+)', output)
    assert(None != m)
    val = int(m.group(1))
    print "%s = %d" % (key, val) 
    d[key] = val
    return val


# calc_mean_var_xo()
# exclude outliers that are more than one std dev from mean, and
# recalculate mean/var
#_______________________________________________________________________________
def calc_mean_var_xo(crops, keyA, keyB, mean_full, std_dev_full):
    mean_xo = 0.0
    num_leafs = 0
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
    
    var_xo = 0

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

def parse_attrs(output):
    """Auxiliary method which parses output of:
    $ ./autoCropScribe imgfp rotateDir
    """
    m = re.search('skewMode: (\w+)', output)
    assert(None != m)
    skewMode = m.group(1)
    print "skewMode is " + skewMode
    
    m = re.search('angle: ([-.\d]+)', output)
    assert(None != m)
    textSkew = float(m.group(1))
    
    m = re.search('conf: ([-.\d]+)', output)
    assert(None != m)
    textScore = float(m.group(1))

    m = re.search('bindingAngle: ([-.\d]+)', output)
    assert(None != m)
    bindingSkew = float(m.group(1))
    
    m = re.search('grayMode: ([\w-]+)', output)
    grayMode = m.group(1)
    print "grayMode is " + grayMode
    
    return skeyMode, textSkew, textScore, bindingSkey, grayMode


def auto_crop_pass1(bid, leafs, jpg_dir):
    """
    """
    def get_jpg(path_id, leafNum, jpg_dir):
        """Auxiliary method to resolve path of jpg"""
        _path = '%s/%s_orig_%04d' % (jpg_dir, _id, leafNum)
        jpg = pipes.quote('%s.JPG' % _path) 
        if not os.path.exists(jpg):
            jpg = pipes.quote('%s.jpg' % _path)
        
        assert os.path.exists(jpg)    
        return jpg

    crops = {}

    for leaf in leafs:
        leafNum = int(leaf.get('leafNum'))
        print 'Processing leaf %d, pass 1 ' % leafNum

        pageType = leaf.findtext('pageType')     
        if pageType in PAGE_BLACKLIST:
            print 'skipping pageType = ' + pageType
            continue #skip first deleted page

        rotateDegree = int(leaf.findtext('rotateDegree'))
        assert(abs(rotateDegree) == 90)
        rotateDir = rotateDegree / 90
        print "rotateDir = %d" % rotateDir

        # XXX - C bindings should be written rather than cmd
        impfp = get_jpg(bid, leafNum, jpg_dir)
        cmd = "./autoCropScribe %s %d" % (imgfp, rotateDir)
        print cmd
        retval, output = commands.getstatusoutput(cmd)
        if 0 != retval:
            print "retval is %d" % (retval)
            print output
            
        assert (0 == retval)
    
        skeyMode, textSkew, textScore, bindingSkey, grayMode = parse_attrs()

        crops[leafNum] = {}
        for croptype in ['Outer', 'Clean', 'Inner']:
            for border in ['L', 'R', 'T', 'B']:
                parse_int('%sCrop%s' % (croptype, border), output, crops[leafNum])

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
    width_mean_full, width_var_full     = mean_var(crops, 'CleanCropL', 'CleanCropR')
    height_mean_full, height_var_full   = mean_var(crops, 'CleanCropT', 'CleanCropB')
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
        if pageType in PAGE_BLACKLIST:
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

        rmtag('skewAngle', leaf)
        rmtag('skewAngleDetect', leaf)
        rmtag('skewScore', leaf)
        rmtag('skewActive', leaf)
        
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
    def set_autocrop_version(scandata):
        """Injects the correct version number into the xmltree under
        the 'autoCropVersion' field
        """
        bookdata = scandata.find(BOOK_DATA)
        rmtag('autoCropVersion', bookdata)
        ET.SubElement(bookdata, 'autoCropVersion').text = str(__version__)

    parser = argparser()
    args = parser.parse_args()
    xmltree = parsexml(args.xml)

    scandata = xmltree.getroot()
    bid = scandata.findtext(BOOK_ID)
    pages = scandata.findall(PAGES)
    set_autocrop_version(scandata)

    crops = auto_crop_pass1(bid, pages, args.imgdir)
    auto_crop_pass2(pages, crops)
    
    xmltree.write(args.xml, pretty_print=True)
