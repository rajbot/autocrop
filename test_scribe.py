#!/usr/bin/env python
#-*- coding: utf-8 -*-

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

    def test_crop_score(self):
        crop_score = ps.score(4, 2, 1)
        self.assertTrue(crop_score == 3.0,
                        "Expected crop_score to return 3.0, not %s" % crop_score)
