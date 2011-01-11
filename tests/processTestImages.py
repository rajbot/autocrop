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

testimg_dir = 'testimg'
testrun_dir = '/home/rkumar/public_html/autocrop/testrun'
today_dir   = testrun_dir + '/' + datetime.date.today().isoformat()
out_dir     = today_dir + '/' + datetime.datetime.now().isoformat()
autocrop    = '/home/rkumar/gnubook/autoCropScribe'

proxy_dir   = 'proxy'
box_dir     = 'box'
cropped_dir = 'cropped'

assert os.path.exists(testimg_dir)
assert not os.path.exists(out_dir)

files = glob.glob(testimg_dir + "/*.jpg")
files += glob.glob(testimg_dir + "/*.JPG")
assert len(files) > 0

if not os.path.exists(today_dir):
    os.mkdir(today_dir)
    
os.mkdir(out_dir)
os.mkdir(out_dir + '/' + proxy_dir)
os.mkdir(out_dir + '/' + box_dir)
os.mkdir(out_dir + '/' + cropped_dir)

latest_link = testrun_dir + '/latest'
if os.path.exists(latest_link):
    os.unlink(latest_link)
os.symlink(out_dir, latest_link)    


print "Creating test output in " + out_dir

out_html = out_dir + '/index.html'
f = open(out_html, 'w')
f.writelines(['<html>\n', '<head><title>autocrop test for %s</title></head>\n'%datetime.datetime.now().isoformat(), '<body>\n', '<table border=2>\n']);

for file in sorted(files):
    print "Processing %s"%(file)
    base_file = os.path.basename(file)
    
    cmd = "/home/rkumar/gnubook/autoCropScribe %s %d" % (file, 1)
    print cmd
    retval,output = commands.getstatusoutput(cmd)
    assert (0 == retval)
    
    retval = commands.getstatusoutput('cp /home/rkumar/public_html/out.jpg "%s/%s/%s"'%(out_dir, proxy_dir, base_file))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp /home/rkumar/public_html/outbox.jpg "%s/%s/%s"'%(out_dir, box_dir, base_file))[0]
    assert (0 == retval)    

    retval = commands.getstatusoutput('cp /home/rkumar/public_html/outcrop.jpg "%s/%s/%s"'%(out_dir, cropped_dir, base_file))[0]
    assert (0 == retval)    

    f.write('<tr>\n')
    f.write('<td valign=top>'),
    f.write('<img src="%s/%s"/>'%(proxy_dir, base_file))
    f.write('</td>\n')

    f.write('<td valign=top>'),
    f.write('<img src="%s/%s"/>'%(box_dir, base_file))
    f.write('</td>\n')


    f.write('<td valign=top>'),
    f.write('<img src="%s/%s"/>'%(cropped_dir,base_file))
    #     f.write("<br>leaf %d"%(leafNum))
    #     f.write("<br>skewMode: %s"%(skewMode))
    #     f.write("<br>textSkew: %0.2f, textScore: %0.2f"%(textSkew, textScore))
    #     f.write("<br>bindingSkew: %0.2f"%(bindingSkew))
    #     f.write("<br>grayMode: %s"%(grayMode))
    f.write('</td>\n')

    f.write('</tr>\n')
    f.flush()

f.writelines(['</table>\n', '</html>\n'])
f.close()
