#!/usr/bin/python

"""
This script creates index.html in the current dir with links to skew.html files
in subdirectories.
"""

import os
import glob

assert not os.path.exists('auto.html')

files = glob.glob("*")
assert len(files) > 0

f = open('auto.html', 'w')
f.writelines(['<html>\n', '<head><title>autocrop test</title></head>\n', '<body>\n']);

totalImages = 0;
totalBooks  = 0;

for file in sorted(files):
    if ('testbooks' == file):
        continue
    if ('index.html' == file):
        continue
    if ('index.html~' == file):
        continue
        
    print file,

    origDir = 'testbooks/%s_orig_jp2'%(file)
    assert os.path.exists(origDir)
    numImages = len(glob.glob(origDir+'/*.jpg'))
    totalImages += numImages

    assert os.path.exists(file + '/skew.html')
    url = 'http://dev01.sheridan.archive.org/autocrop/%s/skew.html' % file
    f.writelines(['<a href="%s">%s</a> - %d images<br>\n'%(url, file, numImages)])

    
    origDir = 'testbooks/%s_orig_jp2'%(file)
    assert os.path.exists(origDir)

    print ' - %d images'%numImages
    
    totalBooks += 1
    
f.writelines(['<br>total number of images tested: %d\n'%totalImages, '<br>total number of books: %d\n'%totalBooks, '</html>\n'])
f.close()    

print 'totalNumber of images = %d' % totalImages
print 'totalNumber of books = %d' % totalBooks