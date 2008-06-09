#!/usr/bin/python

"""
This script autocrops jpg images in the current directory.
"""

import commands
import glob
import os

files = glob.glob("*.jpg")
assert len(files) > 0

outDir     = '/home/rkumar/public_html/autocrop/nouveautraitde00bout'
proxyDir   = 'proxy'
skewedDir  = 'skewed'
croppedDir = 'cropped'

assert not os.path.exists(outDir)
assert not os.path.exists(proxyDir)
assert not os.path.exists(skewedDir)
assert not os.path.exists(croppedDir)

os.mkdir(outDir)
os.mkdir(proxyDir)
os.mkdir(skewedDir)
os.mkdir(croppedDir)

outHtml = 'skew.html'
assert not os.path.exists(outHtml)

f = open(outHtml, 'w')
f.writelines(['<html>\n', '<head><title>skew test for %s</title></head>\n'%os.path.basename(os.getcwd()), '<body>\n', '<table border=2>\n']);

leafNum = 0
rotateDir = -1
for file in sorted(files):
    print "Processing %s, leafNum=%d, rotateDir=%d"%(file, leafNum, rotateDir)
    
    cmd = "~/gnubook/autoCrop %s %d" % (file, rotateDir)
    print cmd
    retval = commands.getstatusoutput(cmd)[0]
    assert (0 == retval)

    retval = commands.getstatusoutput('cp ~/public_html/out.jpg "%s/%d.jpg"'%(proxyDir,leafNum))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp ~/public_html/outbox.jpg "%s/%d.jpg"'%(skewedDir,leafNum))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp ~/public_html/outcrop.jpg "%s/%d.jpg"'%(croppedDir,leafNum))[0]
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
    f.write('</td>\n')

    f.write('</tr>\n')
    
    if (1==rotateDir):
        rotateDir = -1
    else:
        rotateDir = 1
        
    leafNum+=1
    
    #if (3==leafNum):
    #    break
    
f.writelines(['</table>\n', '</html>\n'])
f.close()

retval = commands.getstatusoutput('cp %s "%s"'%(outHtml,outDir))[0]
assert (0 == retval)    
retval = commands.getstatusoutput('cp -r %s "%s"'%(proxyDir,outDir))[0]
assert (0 == retval)    
retval = commands.getstatusoutput('cp -r %s "%s"'%(skewedDir,outDir))[0]
assert (0 == retval)    
retval = commands.getstatusoutput('cp -r %s "%s"'%(croppedDir,outDir))[0]
assert (0 == retval)    
