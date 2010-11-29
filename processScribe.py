#!/usr/bin/python

"""
This script preprocesses images from an IA Scribe bookscanner.
For each image found in scandata.xml, it calculates a cropbox
and skew angle, and writes the results back into scandata.xml.
These can later be adjusted manually in Republisher.
"""

import commands
import os
import re
import sys
#import xml.etree.ElementTree as ET
from lxml import etree as ET

if 3 != len(sys.argv):
    sys.exit('Usage: %s scandata.xml jpg_dir' % sys.argv[0])

scandata_xml    = sys.argv[1]
jpg_dir         = sys.argv[2]
autocrop_bin    = os.path.expanduser('~') + '/gnubook/autoCropScribe'

assert os.path.exists(scandata_xml)
assert os.path.exists(jpg_dir)

parser          = ET.XMLParser(remove_blank_text=True) #to enable pretty_printing later
scandata_etree  = ET.parse(scandata_xml, parser)
scandata        = scandata_etree.getroot()
id              = scandata.findtext('bookData/bookId')
leafs           = scandata.findall('.//page')

print 'Autocropping ' + id

def getFloatStr(label, output):
    m=re.search('%s: ([-.\d]+)'%label, output)
    assert(None != m)
    print "%s is %s" % (label, m.group(1))
    return m.group(1)

def removeElements(tag, parent):
    elements = parent.findall(tag)
    for element in elements:
        parent.remove(element)
        
def addCropBox(tag, parent, x, y, w, h):
    cropBox = ET.SubElement(parent, tag)
    ET.SubElement(cropBox, 'x').text = x
    ET.SubElement(cropBox, 'y').text = y
    ET.SubElement(cropBox, 'w').text = w
    ET.SubElement(cropBox, 'h').text = h


#__main()__
#______________________________________________________________________________

for leaf in leafs:
    leafNum = int(leaf.get('leafNum'))
    rotateDegree = int(leaf.findtext('rotateDegree'))
    print 'Processing leaf %d ' % leafNum
    
    if 'Delete' == leaf.findtext('pageType'):
        print ' This is a deleted page, skipping!'
        continue
    
    assert (90 == rotateDegree) or (-90 == rotateDegree)
    
    if 90 == rotateDegree:
        rotateDir = 1
    else:
        rotateDir = -1
        
    jpg = '%s_orig_%04d.jpg' % (id, leafNum)
    assert os.path.exists(jpg)
    
    cmd = "/home/rkumar/gnubook/autoCropScribe %s %d" % (jpg, rotateDir)
    print cmd
    retval,output = commands.getstatusoutput(cmd)
    assert (0 == retval)
    #print output

    m=re.search('skewMode: (\w+)', output)
    skewMode = m.group(1)
    print "skewMode is " + skewMode

    m=re.search('grayMode: ([\w-]+)', output)
    grayMode = m.group(1)
    print "grayMode is " + grayMode

        
    skewAngle = getFloatStr('angle', output)
    skewScore = getFloatStr('conf', output)
    cropX = getFloatStr('cropX', output)
    cropY = getFloatStr('cropY', output)
    cropW = getFloatStr('cropW', output)
    cropH = getFloatStr('cropH', output)
    
    removeElements('cropBox', leaf)
    removeElements('cropBoxAutoDetect', leaf)
    
    addCropBox('cropBox', leaf, cropX, cropY, cropW, cropH)    
    addCropBox('cropBoxAutoDetect', leaf, cropX, cropY, cropW, cropH)
    ET.SubElement(leaf, 'cropScore').text = '1.0' #TODO: we are fabricating a crop box autodetect score for now
    
    removeElements('skewAngle', leaf)
    removeElements('skewAngleAutoDetect', leaf)
    removeElements('skewScore', leaf)
    removeElements('skewActive', leaf)
    
    #set skewAngle to autoDetect angle, regardless of score
    ### TODO: the returned score is currently only the textSkew score, but sometimes we use the binding angle
    ET.SubElement(leaf, 'skewAngle').text = skewAngle
    ET.SubElement(leaf, 'skewAngleDetect').text = skewAngle
    ET.SubElement(leaf, 'skewScore').text = skewScore
    ET.SubElement(leaf, 'skewActive').text = 'true' #TODO: always set this to true, regardless of score.
    
scandata_etree.write(scandata_xml, pretty_print=True)
