#!/usr/bin/python

"""
This script sets all cropboxes of Normal pages to be the same as the first page
"""

import commands
import glob
import os
import re
import sys
import xml.etree.ElementTree as et


assert 3 == len(sys.argv)

infile  = sys.argv[1]
outfile = sys.argv[2]

assert os.path.exists(infile)
assert not os.path.exists(outfile)

root = et.Element("book")
pageData = et.Element("pageData")
root.append(pageData)

gotFirstCropRight = 0
gotFirstCropLeft  = 0


scandata = et.parse(infile).getroot()
leafs = scandata.findall('.//page')
for oldleaf in leafs:
    print "processing leaf %s" % (oldleaf.attrib["leafNum"])
    
    #newleaf = et.Element("page")
    #pageData.append(newleaf)    
    #newleaf.set("leafNum", oldleaf.attrib["leafNum"])

    pageType = oldleaf.findtext('pageType')
    handSide = oldleaf.findtext('handSide')
    cropBox  = oldleaf.find('cropBox')
    cropX    = cropBox.find('x')
    cropY    = cropBox.find('y')
    cropW    = cropBox.find('w')
    cropH    = cropBox.find('h')    
    print "handSide = " + handSide
    if ("RIGHT" == handSide) and (("Normal" == pageType) or ("Copyright" == pageType) or ("Title" == pageType)):
        if (gotFirstCropRight):
            cropX.text = rX
            cropY.text = rY
            cropW.text = rW
            cropH.text = rH            
        else:
            rX = cropX.text
            rY = cropY.text
            rW = cropW.text
            rH = cropH.text
            print "set right x,y w,h = %s,%s %s,%s"%(rX,rY,rW,rH)
            gotFirstCropRight = 1 
    elif ("LEFT" == handSide) and (("Normal" == pageType) or ("Copyright" == pageType) or ("Title" == pageType)):
        if (gotFirstCropLeft):
            cropX.text = lX
            cropY.text = lY
            cropW.text = lW
            cropH.text = lH            
        else:
            lX = cropX.text
            lY = cropY.text
            lW = cropW.text
            lH = cropH.text
            print "set left x,y w,h = %s,%s %s,%s"%(lX,lY,lW,lH)
            gotFirstCropLeft = 1
    
    pageData.append(oldleaf)    
    

et.ElementTree(root).write(outfile, 'utf-8')