
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
        self.assertTrue(len(xmltree.findall("child")) == 2)
        ps.rmtag("child", xmltree.getroot())
        self.assertTrue(not len(xmltree.findall("child")))

    def test_addCropBox(self):
        xmltree = etree.parse(StringIO(XML['rmtag']))
        tag = "child"
        parent = xmltree.getroot()
        xywh = [1, 2, 3, 4]
        ps.addCropBox(tag, parent, *xywh)
        child = xmltree.getroot().findall("child")[0]
        for tagname, val in zip(['x', 'y', 'w', 'h'], xywh):
            self.assertTrue(len(child.findall(tagname)),
                            "Expected tag to have children")
            intval = int(child.findall(tagname)[0].text)
            self.assertTrue(intval == val, "Tag '%s' expected to have val %s, " \
                                "instead has val %s" % (tagname, val, intval))
    
    def test_crop_score(self):
        crop_score = ps.crop_score(4, 2, 1)
        self.assertTrue(crop_score == 3.0,
                        "Expected crop_score to return 3.0, not %s" % crop_score)
    
    def test_mean_var(self):
        crops = [{'a': 3, 'b': 6}, {'a': 2, 'b': 5}, {'a': 5, 'b': 3}]
        mean, var = ps.mean_var(crops, 'a', 'b')
        self.assertTrue("%.2f" % mean == str(2.33) and "%.2f" % var == str(5.56),
                        "Expected mean=2.33, var=5.55 got mean=%.2f, var=%.2f" % \
                            (mean, var))
