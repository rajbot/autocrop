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

scanDataFile   = sys.argv[1]
templateSpread = int(sys.argv[2])

assert os.path.exists(scanDataFile)

cropTemplateL = templateSpread*2
cropTemplateR = cropTemplateL + 1

scanData = et.parse(scanDataFile).getroot()
bookData = scanData.find('bookData')
assert None == bookData.find('cropMode')
assert None == bookData.find('cropTemplateL')
assert None == bookData.find('cropTemplateR')

cropMode = et.Element("cropMode")
cropMode.text = 'auto'
bookData.append(cropMode)

cropL = et.Element("cropTemplateL")
cropL.text = str(cropTemplateL)
bookData.append(cropL)

cropR = et.Element("cropTemplateR")
cropR.text = str(cropTemplateR)
bookData.append(cropR)

et.ElementTree(scanData).write(scanDataFile, 'utf-8')
