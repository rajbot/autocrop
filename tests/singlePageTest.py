#!/usr/bin/python

"""
This script autocrops jpg images in the testpages directory.
It is for testing single pages, as opposed to book pages.
"""

import commands
import glob
import os
import re

assert os.path.exists('testpages')
files = glob.glob("testpages/*.jpg")
assert len(files) > 0

outDir     = 'singlepages'
proxyDir   = 'proxy'
skewedDir  = 'skewed'
croppedDir = 'cropped'

assert not os.path.exists(outDir)
assert not os.path.exists(outDir + '/' + proxyDir)
assert not os.path.exists(outDir + '/' + skewedDir)
assert not os.path.exists(outDir + '/' + croppedDir)

os.mkdir(outDir)
os.mkdir(outDir + '/' + proxyDir)
os.mkdir(outDir + '/' + skewedDir)
os.mkdir(outDir + '/' + croppedDir)

outHtml = outDir + '/skew.html'
assert not os.path.exists(outHtml)


f = open(outHtml, 'w')
f.writelines(['<html>\n', '<head><title>skew test for single pages</title></head>\n', '<body>\n', '<table border=2>\n']);

for file in sorted(files):
    m=re.search('_orig_(\d{4})\.jpg', file)
    assert(None != m)
    leafNum = int(m.group(1))

    if (0 == (leafNum & 0x1)):
        rotateDir = -1;
    else:
        rotateDir = 1;
        
    print "Processing %s, leafNum=%d, rotateDir=%d"%(file, leafNum, rotateDir)
    
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

    f.write('</tr>\n')
    f.flush()
    
    
f.writelines(['</table>\n', '</html>\n'])
f.close()
