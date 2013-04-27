
"""
    scribe_test
    ~~~~~~~~~~~

    basic test cases for processScribe
"""

from lxml import etree
from StringIO import StringIO
import unittest
from processScribe import rmtag

XML = {'rmtag': "<parent><child>1</child><child>2</child></parent>"}       

class TestScribe(unittest.TestCase):

    def test_rmtag(self):
        xmltree = etree.parse(StringIO(XML['rmtag']))
        assert(len(xmltree.findall("child")) == 2)
        rmtag("child", xmltree.getroot())
        assert(not len(xmltree.findall("child")))



