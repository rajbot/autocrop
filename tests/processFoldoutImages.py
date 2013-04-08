#!/usr/bin/python

"""
This script preprocesses images from an IA Scribe bookscanner.
For each image found in the testimg directory, it calculates
a cropbox and skew angle, and writes the results back into outdir.
"""

import commands
import os
import re
import sys
import glob
import datetime
from os.path import join, exists

testimg_dir = 'testimg_foldout'
testrun_dir = 'testrun'
today       = datetime.date.today().isoformat()
now         = datetime.datetime.now().isoformat()
today_dir   = join(testrun_dir, today)
out_dir     = join(testrun_dir, today, now)
autocrop    = '../autoCropFoldout'

proxy_dir   = 'proxy'
box_dir     = 'box'
cropped_dir = 'cropped'
debug_img_dir = 'debug-images'

assert exists(testimg_dir)
assert not exists(out_dir)

files = glob.glob(testimg_dir + "/*.jpg")
files += glob.glob(testimg_dir + "/*.JPG")
assert len(files) > 0

if not exists(today_dir):
    os.mkdir(today_dir)

os.mkdir(out_dir)
os.mkdir(join(out_dir, proxy_dir))
os.mkdir(join(out_dir, box_dir))
os.mkdir(join(out_dir, cropped_dir))

if not exists(debug_img_dir):
    os.mkdir(debug_img_dir)

latest_link = join(testrun_dir, 'latest')
if os.path.lexists(latest_link):
    ret = os.unlink(latest_link)
os.symlink(join(today, now), latest_link)


# move()
#_________________________________________________________________________________________
def move(src, dest):
    try:
        os.rename(src, dest)
    except OSError:
        sys.exit('Could not move debug image %s. Please make sure you compile with `make CXXFLAGS=-DWRITE_DEBUG_IMAGES`' % src)


# __main__
#_________________________________________________________________________________________
print "Creating test output in " + out_dir

out_html = out_dir + '/index.html'
f = open(out_html, 'w')
f.writelines(['<html>\n', '<head><title>autocrop test for %s</title></head>\n'%datetime.datetime.now().isoformat(), '<body>\n', '<table border=2>\n']);

for file in sorted(files):
    print "Processing %s"%(file)
    base_file = os.path.basename(file)

    m = re.search('(\d{4})\.(jpg|JPG)$', file)
    leafnum = int(m.group(1))
    print 'leafNum is %d' % leafnum

    cmd = "%s %s" % (autocrop, file)
    print cmd
    retval,output = commands.getstatusoutput(cmd)
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

    m=re.search('grayMode: ([\w-]+)', output)
    grayMode = m.group(1)
    print "grayMode is " + grayMode

    #move debug images from debug_img_dir into outdir
    move(join(debug_img_dir, 'out.jpg'),     join(out_dir, proxy_dir, base_file))
    move(join(debug_img_dir, 'outbox.jpg'),  join(out_dir, box_dir, base_file))
    move(join(debug_img_dir, 'outcrop.jpg'), join(out_dir, cropped_dir, base_file))

    f.write('<tr>\n')
    f.write('<td valign=top>'),
    f.write('<img src="%s/%s"/>'%(proxy_dir, base_file))
    f.write("<br>leaf %s"%(base_file))
    f.write("<br>skewMode: %s"%(skewMode))
    f.write("<br>textSkew: %0.2f, textScore: %0.2f"%(textSkew, textScore))
    f.write("<br>grayMode: %s"%(grayMode))
    f.write('</td>\n')

    f.write('<td valign=top>'),
    f.write('<img src="%s/%s"/>'%(box_dir, base_file))
    f.write('</td>\n')


    f.write('<td valign=top>'),
    f.write('<img src="%s/%s"/>'%(cropped_dir,base_file))
    f.write('</td>\n')

    f.write('</tr>\n')
    f.flush()

f.writelines(['</table>\n', '</html>\n'])
f.close()
