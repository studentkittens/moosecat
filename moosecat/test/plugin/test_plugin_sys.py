# encoding: utf-8

"""
A Simple Test for the plugin system

.. moduleauthor:: serztle <serztle@googlemail.com>
"""

import unittest
import pprint
from pprint import pprint as print
from moosecat.plugin.plugin_sys import PluginSystem


class TestPluginSystem(unittest.TestCase):
    def setUp(self):
        print("setUp")
        self.psys = PluginSystem()
        self.psys.scan()
        #self.psys.load('glyr')
        #self.psys.load('lastfm')
        print(self.psys.__dict__)

    def test_load_by_name(self):
        print("test_load_by_name")
        #print(self.psys._module_list)

        self.psys.load('glyr')
        self.psys.load('lastfm')

        print(self.psys._loaded_list)
        print(len(self.psys._loaded_list))
        self.assertTrue(len(self.psys._loaded_plugins) == 2)

    def test_load_by_tag(self):
        pass

    def tearDown(self):
        self.psys = None


#if __name__ == '__main__':
    #unittest.main()
