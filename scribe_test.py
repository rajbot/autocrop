
"""
    scribe_test
    ~~~~~~~~~~~

    basic test cases for processScribe
"""

from lxml import etree
from StringIO import StringIO
import unittest
import processScribe as ps

XML = {'rmtag': "<parent><child>1</child><child>2</child></parent>"}       

class TestScribe(unittest.TestCase):

    def test_rmtag(self):        
        xmltree = etree.parse(StringIO(XML['rmtag']))
        assert(len(xmltree.findall("child")) == 2)
        ps.rmtag("child", xmltree.getroot())
        assert(not len(xmltree.findall("child")))

    def test_addCropBox(self):
        xmltree = etree.parse(StringIO(XML['rmtag']))
        tag = "child"
        parent = xmltree.getroot()
        xywh = [1, 2, 3, 4]
        ps.addCropBox(tag, parent, *xywh)
        child = xmltree.getroot().findall("child")[0]
        for tagname, val in zip(['x', 'y', 'w', 'h'], xywh):
            assert(len(child.findall(tagname)))
            assert(int(child.findall(tagname)[0].text) == val)
