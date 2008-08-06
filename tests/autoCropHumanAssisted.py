#!/usr/bin/python

"""
This script autocrops jpg images in the current directory.
"""

import commands
import glob
import os
import re
import xml.etree.ElementTree as ET

def parseCrops(output):
    m=re.search('cropX=(\d+)', output)
    cropX = int(m.group(1))
    
    m=re.search('cropY=(\d+)', output)
    cropX = int(m.group(1))
    
    m=re.search('cropW=(\d+)', output)
    cropX = int(m.group(1))
    
    m=re.search('cropH=(\d+)', output)
    cropX = int(m.group(1))

    print "crop xy, wh = %d,%d   %d,%d\n" % (cropx, cropy, cropw, croph)
    return (cropx, cropy, cropw, croph)

def parseGaps(output):
    m=re.search('bindingGap: (\d+)', output)
    bindingGap = int(m.group(1))

    m=re.search('topGap: (\d+)', output)
    topGap = int(m.group(1))
    
    m=re.search('bottomGap: (\d+)', output)
    bottomGap = int(m.group(1))

    print "gap binding=%d top=%d bottom=%d\n" % (bindingGap, topGap, bottomGap)
    return (bindingGap, topGap, bottomGap)


def getBindingGaps(file, rotDir, skew, cropx, cropy, cropw, croph):
    cmd = "/home/scribe/petamnt/gnubook/autoCropHumanAssisted %s %d %f %d %d %d %d" % (file, rotateDir, skew, cropx, cropy, cropw, croph)
    print cmd
    retval,output = commands.getstatusoutput(cmd)
    assert (0 == retval)
    
    return parseGaps(output)

# main()
#______________________________________________________________________________

files = glob.glob("*.jpg")
assert len(files) > 0

outDir      = '/var/www/humanassist/' + os.path.basename(os.getcwd()).split('_')[0]
#outDir     = '/home/rkumar/public_html/autocrop/picturesquenewen00swee'
#outDir     = '/home/rkumar/public_html/autocrop/nouveautraitde00bout'
#outDir     = '/home/rkumar/public_html/autocrop/morritosentrem00alva'
#outDir     = '/home/rkumar/public_html/autocrop/appendixtotheolo00painrich'
proxyDir   = 'proxy'
skewedDir  = 'skewed'
croppedDir = 'cropped'
#humanDir   = 'human'

assert not os.path.exists(outDir)
assert not os.path.exists(outDir + '/' + proxyDir)
assert not os.path.exists(outDir + '/' + skewedDir)
assert not os.path.exists(outDir + '/' + croppedDir)
#assert not os.path.exists(outDir + '/' + humanDir)
assert os.path.exists('scandata.xml');

os.mkdir(outDir)
os.mkdir(outDir + '/' + proxyDir)
os.mkdir(outDir + '/' + skewedDir)
os.mkdir(outDir + '/' + croppedDir)
#os.mkdir(outDir + '/' + humanDir)

outHtml = outDir + '/skew.html'
assert not os.path.exists(outHtml)

scandata = ET.parse('scandata.xml').getroot()
leafs = scandata.findall('.//page')

f = open(outHtml, 'w')
f.writelines(['<html>\n', '<head><title>human assist skew test for %s</title></head>\n'%os.path.basename(os.getcwd()), '<body>\n', '<table border=2>\n']);

leafNum = 0
rotateDir = -1

gotInitCropBoxL  = 0
gotInitCropBoxR  = 0

for file in sorted(files):
    print "Processing %s, leafNum=%d, rotateDir=%d"%(file, leafNum, rotateDir)
    if (0 == leafNum):
        leafNum+=1
        rotateDir = 1
        continue

    autoCropThisPage = 0
    
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

    rotateDegree = int(leaf.findtext('rotateDegree'))
    assert((-90 == rotateDegree) or (90 == rotateDegree))
    pageType = leaf.findtext('pageType')
    
    if ("Normal" == pageType) or ("Title" == pageType) or ("Copyright" == pageType):
        if (-90 == rotateDegree):
            if (0 == gotInitCropBoxL):
                cropLx = cropx
                cropLy = cropy
                cropLw = cropw
                cropLh = croph
                gotInitCropBoxL = 1
                (bindingGapL, topGapL, bottomGapL) = getBindingGaps(file, rotDir, skew, cropx, cropy, cropw, croph)
            else:
                autoCropThisPage = 1
        else:
            if (0 == gotInitCropBoxR):
                cropRx = cropx
                cropRy = cropy
                cropRw = cropw
                cropRh = croph
                (bindingGapR, topGapR, bottomGapR) = getBindingGaps(file, rotDir, skew, cropx, cropy, cropw, croph)
            else:
                autoCropThisPage = 1
    
    
    if (0 == autoCropThisPage):
        retval = commands.getstatusoutput('/home/scribe/petamnt/gnubook/tests/cropAndSkewProxy %s "%s/%s/%d.jpg" %d %f %d %d %d %d'%(file, outDir, croppedDir,leafNum, rotateDir, skew, cropx/8, cropy/8, cropw/8, croph/8))[0]
        assert (0 == retval)    

    else:
        if (-90 == rotateDegree):
            cmd = "/home/scribe/petamnt/gnubook/autoCropHumanAssisted %s %d %f %d %d %d %d" % (file, rotateDir, skew, cropLx, cropLy, cropLw, cropLh)
            print cmd
            retval,output = commands.getstatusoutput(cmd)
            assert (0 == retval)
            
            (bindingGapL, topGapL, bottomGapL) = parseGaps(output)
            (cropLx, cropLy, cropLw, cropLh)   = parseCrops(output)
        else:
            cmd = "/home/scribe/petamnt/gnubook/autoCropHumanAssisted %s %d %f %d %d %d %d" % (file, rotateDir, skew, cropRx, cropRy, cropRw, cropRh)
            print cmd
            retval,output = commands.getstatusoutput(cmd)
            assert (0 == retval)
            
            (bindingGapR, topGapR, bottomGapR) = parseGaps(output)
            (cropRx, cropRy, cropRw, cropRh)   = parseCrops(output)

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
    
        m=re.search('grayMode: ([\w-]+)', output)
        grayMode = m.group(1)
        print "grayMode is " + grayMode
            
        retval = commands.getstatusoutput('cp /tmp/home/rkumar/out.jpg "%s/%s/%d.jpg"'%(outDir, proxyDir,leafNum))[0]
        assert (0 == retval)    
    
        retval = commands.getstatusoutput('cp /tmp/home/rkumar/outbox.jpg "%s/%s/%d.jpg"'%(outDir, skewedDir,leafNum))[0]
        assert (0 == retval)    
    
        retval = commands.getstatusoutput('cp /tmp/home/rkumar/outcrop.jpg "%s/%s/%d.jpg"'%(outDir, croppedDir,leafNum))[0]
        assert (0 == retval)    


    f.write('<tr>\n')
    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(proxyDir,leafNum))
    f.write('</td>\n')

    f.write('<td valign=top>'),
    if (1 == autoCropThisPage):
        f.write('<img src="%s/%d.jpg"/>'%(skewedDir,leafNum))
    else:
        f.write('&nbps;')
    f.write('</td>\n')


    f.write('<td valign=top>'),
    f.write('<img src="%s/%d.jpg"/>'%(croppedDir,leafNum))
    f.write("<br>leaf %d"%(leafNum))
    if (1 == autoCropThisPage):
        f.write("<br>skewMode: %s"%(skewMode))
        f.write("<br>textSkew: %0.2f, textScore: %0.2f"%(textSkew, textScore))
        f.write("<br>bindingSkew: %0.2f"%(bindingSkew))
        f.write("<br>grayMode: %s"%(grayMode))
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
