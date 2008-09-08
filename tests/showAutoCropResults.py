#!/usr/bin/python

"""
This script autocrops jpg images in the current directory.
"""

import commands
import glob
import os
import re
import xml.etree.ElementTree as ET

# main()
#______________________________________________________________________________

bookId = os.path.basename(os.getcwd())
print "bookId is " + bookId

origJp2Dir = "%s_orig_jp2" % bookId

print "untarring..."
if not os.path.exists(origJp2Dir):
    (retval, output) = commands.getstatusoutput('tar -xvf %s.tar'%origJp2Dir)
    print output
    assert (0 == retval)    

assert os.path.exists(origJp2Dir)

files = glob.glob("%s/*.jp2"%origJp2Dir)
assert len(files) > 0

scandataFile = '%s_scandata.xml'%bookId
assert os.path.exists(scandataFile);

outDir      = '/var/www/humanassist/' + bookId
croppedDir   = 'cropped'
proxyDir   = 'proxy'
croppedDir = 'cropped'

assert not os.path.exists(outDir)
assert not os.path.exists(outDir + '/' + proxyDir)
assert not os.path.exists(outDir + '/' + croppedDir)

os.mkdir(outDir)
os.mkdir(outDir + '/' + proxyDir)
os.mkdir(outDir + '/' + croppedDir)


outHtml = outDir + '/skew.html'
assert not os.path.exists(outHtml)

scandata = ET.parse(scandataFile).getroot()
leafs = scandata.findall('.//page')

cropTemplateL = int(scandata.findtext('bookData/cropTemplateL'))
cropTemplateR = int(scandata.findtext('bookData/cropTemplateR'))

#print "cropTemplates: L=%d, R=%d"%(cropTemplateL, cropTemplateR)

f = open(outHtml, 'w')
f.writelines(['<html>\n', '<head><title>human assist skew test for %s</title></head>\n'%os.path.basename(os.getcwd()), '<body>\n', '<table border=2>\n']);

f.write('<tr>\n')
f.write('<td valign=top>'),
f.write("Left Crop Template (leaf %d):<br/>\n"%cropTemplateL);
f.write('<img src="%s/%d.jpg"/>'%(proxyDir,cropTemplateL))
f.write('</td>\n')

f.write('<td valign=top>'),
f.write("Left Crop Template (leaf %d):<br/>\n"%cropTemplateL);
f.write('<img src="%s/%d.jpg"/>'%(croppedDir,cropTemplateL))
f.write('</td>\n')
f.write('</tr>\n')

f.write('<tr>\n')
f.write('<td valign=top>'),
f.write("Right Crop Template (leaf %d):<br/>\n"%cropTemplateR);
f.write('<img src="%s/%d.jpg"/>'%(proxyDir,cropTemplateR))
f.write('</td>\n')

f.write('<td valign=top>'),
f.write("Right Crop Template (leaf %d):<br/>\n"%cropTemplateR);
f.write('<img src="%s/%d.jpg"/>'%(croppedDir,cropTemplateR))
f.write('</td>\n')
f.write('</tr>\n')

f.write('</table>\n<br/><br/>\n');

f.write('<table border=2>\n');

leafNum = 0

gotInitCropBoxL  = 0
gotInitCropBoxR  = 0

for file in sorted(files):

    if (0 == leafNum):
        leafNum+=1
        continue

    
    leaf=leafs[leafNum]
    
    if ('RIGHT' == leaf.findtext('handSide')):
        rotateDir = 1        
        #leafNum+=1;
        #continue
    else:
        rotateDir = -1

    print "Processing %s, leafNum=%d, rotateDir=%d"%(file, leafNum, rotateDir)
    
    skew = float(leaf.findtext('skewAngle'))
    #score = float(leaf.findtext('skewScore'))
    skewAct = leaf.findtext('skewActive')
    cropx = int(leaf.findtext('cropBox/x'))
    cropy = int(leaf.findtext('cropBox/y'))
    cropw = int(leaf.findtext('cropBox/w'))
    croph = int(leaf.findtext('cropBox/h'))
    if "false" == skewAct:
        skew = 0.0

    rotateDegree = int(leaf.findtext('rotateDegree'))
    #assert((-90 == rotateDegree) or (90 == rotateDegree))
    pageType = leaf.findtext('pageType')
    
    
    
    retval = commands.getstatusoutput('LD_LIBRARY_PATH=~/kdubin ~/kdubin/kdu_expand -i %s -o tmp.tif'%file)[0]
    assert (0 == retval)    
    
    retval = commands.getstatusoutput('/home/scribe/petamnt/gnubook/tests/cropAndSkewProxy tmp.tif "%s/%s/%d.jpg" %d %f %d %d %d %d 1'%(outDir, croppedDir,leafNum, rotateDir, skew, cropx/8, cropy/8, cropw/8, croph/8))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp /tmp/home/rkumar/out.jpg "%s/%s/%d.jpg"'%(outDir, proxyDir,leafNum))[0]
    assert (0 == retval)    

    f.write('<tr>\n')
    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(proxyDir,leafNum))
    f.write('</td>\n')
    
    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(croppedDir,leafNum))
    f.write("<br>leaf %d"%(leafNum))
    f.write("<br>skewAngle: %f"%(skew))
    if leafNum == cropTemplateL:
        f.write("<br>This is the Left Crop Template!")

    if leafNum == cropTemplateR:
        f.write("<br>This is the Right Crop Template!")
    
    cropMode = leaf.findtext('pageCropMode')
    if 'manual' == cropMode:
        f.write("<br>This page is set to MANUAL CROP!")    
    
    f.write('</td>\n')

    f.write('</tr>\n')
    f.flush()
            
    leafNum+=1
    
    #if (4==leafNum):
    #    break
    
f.writelines(['</table>\n', '</html>\n'])
f.close()

#retval = commands.getstatusoutput('cp %s "%s"'%(outHtml,outDir))[0]
#assert (0 == retval)    
#retval = commands.getstatusoutput('cp -r %s "%s"'%(proxyDir,outDir))[0]
#assert (0 == retval)    
#retval = commands.getstatusoutput('cp -r %s "%s"'%(skewedDir,outDir))[0]
#assert (0 == retval)    
#retval = commands.getstatusoutput('cp -r %s "%s"'%(croppedDir,outDir))[0]
#assert (0 == retval)    
