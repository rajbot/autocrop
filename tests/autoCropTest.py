#!/usr/bin/python

"""
This script autocrops jpg images in the current directory.
"""

import commands
import glob
import os
import re
import xml.etree.ElementTree as ET

files = glob.glob("*.jpg")
assert len(files) > 0

#outDir     = '/home/rkumar/public_html/autocrop/picturesquenewen00swee'
outDir     = '/home/rkumar/public_html/autocrop/nouveautraitde00bout'
#outDir     = '/home/rkumar/public_html/autocrop/morritosentrem00alva'
#outDir     = '/home/rkumar/public_html/autocrop/appendixtotheolo00painrich'
proxyDir   = 'proxy'
skewedDir  = 'skewed'
croppedDir = 'cropped'
humanDir   = 'human'

assert not os.path.exists(outDir)
assert not os.path.exists(outDir + '/' + proxyDir)
assert not os.path.exists(outDir + '/' + skewedDir)
assert not os.path.exists(outDir + '/' + croppedDir)
assert not os.path.exists(outDir + '/' + humanDir)
assert os.path.exists('scandata.xml');

os.mkdir(outDir)
os.mkdir(outDir + '/' + proxyDir)
os.mkdir(outDir + '/' + skewedDir)
os.mkdir(outDir + '/' + croppedDir)
os.mkdir(outDir + '/' + humanDir)

outHtml = outDir + '/skew.html'
assert not os.path.exists(outHtml)

scandata = ET.parse('scandata.xml').getroot()
leafs = scandata.findall('.//page')

f = open(outHtml, 'w')
f.writelines(['<html>\n', '<head><title>skew test for %s</title></head>\n'%os.path.basename(os.getcwd()), '<body>\n', '<table border=2>\n']);

leafNum = 0
rotateDir = -1
for file in sorted(files):
    print "Processing %s, leafNum=%d, rotateDir=%d"%(file, leafNum, rotateDir)
    if (0 == leafNum):
        leafNum+=1;
        rotateDir = 1;
        continue;
    
    cmd = "~/gnubook/autoCropScribe %s %d" % (file, rotateDir)
    print cmd
    retval,output = commands.getstatusoutput(cmd)
    assert (0 == retval)

    m=re.search('skewMode: (\w+)', output)
    skewMode = m.group(1)
    print "skewMode is " + skewMode
    
    m=re.search('angle=([-.\d]+)', output)
    assert(None != m)
    textSkew = float(m.group(1))

    m=re.search('conf=([-.\d]+)', output)
    assert(None != m)
    textScore = float(m.group(1))
    
    m=re.search('bindingAngle=([-.\d]+)', output)
    assert(None != m)
    bindingSkew = float(m.group(1))

    
    retval = commands.getstatusoutput('cp ~/public_html/out.jpg "%s/%s/%d.jpg"'%(outDir, proxyDir,leafNum))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp ~/public_html/outbox.jpg "%s/%s/%d.jpg"'%(outDir, skewedDir,leafNum))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp ~/public_html/outcrop.jpg "%s/%s/%d.jpg"'%(outDir, croppedDir,leafNum))[0]
    assert (0 == retval)    
    
    leaf=leafs[leafNum]
    skew = float(leaf.findtext('skewAngle'))
    #score = float(leaf.findtext('skewScore'))
    skewAct = leaf.findtext('skewActive')
    cropx = int(leaf.findtext('cropBox/x'))
    cropy = int(leaf.findtext('cropBox/y'))
    cropw = int(leaf.findtext('cropBox/w'))
    croph = int(leaf.findtext('cropBox/h'))
    if "false" == skewAct:
        skew = 0.0
    
    #print "crop x,y,w,h = %d,%d %d,%d"%(cropx, cropy, cropw, croph)
    #print '/home/rkumar/gnubook/tests/cropAndSkewProxy %s "%s/%s/%d.jpg" %d %f %d %d %d %d'%(file, outDir, humanDir,leafNum, rotateDir, skew, cropx, cropy, cropw, croph)
    retval = commands.getstatusoutput('/home/rkumar/gnubook/tests/cropAndSkewProxy %s "%s/%s/%d.jpg" %d %f %d %d %d %d'%(file, outDir, humanDir,leafNum, rotateDir, skew, cropx/8, cropy/8, cropw/8, croph/8))[0]
    assert (0 == retval)    

    f.write('<tr>\n')
    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(proxyDir,leafNum))
    f.write('</td>\n')

    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(skewedDir,leafNum))
    f.write('</td>\n')


    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(croppedDir,leafNum))
    f.write("<br>leaf %d"%(leafNum))
    f.write("<br>skewMode: %s"%(skewMode))
    f.write("<br>textSkew: %0.2f, textScore: %0.2f"%(textSkew, textScore))
    f.write("<br>bindingSkew: %0.2f"%(bindingSkew))
    f.write('</td>\n')

    f.write('<td valign=top>'),
    f.write("human crop and skew:<br>")
    f.write('<img src="%s/%d.jpg"/>'%(humanDir,leafNum))
    f.write("<br>Skew: %0.2f, crop x,y w,h = %d,%d %d,%d"%(skew, cropx, cropy, cropw, croph))
    f.write('</td>\n')

    f.write('</tr>\n')
    f.flush()
    
    if (1==rotateDir):
        rotateDir = -1
    else:
        rotateDir = 1
        
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
