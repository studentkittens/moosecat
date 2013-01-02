# encoding: utf-8

"""
A Simple Test for the config

.. moduleauthor:: serztle <serztle@googlemail.com>
"""

import unittest
from moosecat.config.config import Config


class TestConfig(unittest.TestCase):
    def setUp(self):
        self.config = Config('app.yaml')
        self.config.set('is', 'easy')
        self.config.set('foo', {'bar': 'foobar'})
        self.config.set('list', [1, 2, 3, 4])
        self.config.save()
        self.config = Config('app.yaml')

    def test_read(self):
        self.config.load()
        self.assertTrue(self.config.get('is') == 'easy')
        self.assertTrue(self.config.get('foo') == {'bar': 'foobar'})
        self.assertTrue(self.config.get('list') == [1, 2, 3, 4])

    def tearDown(self):
        self.config = None


if __name__ == '__main__':
    unittest.main()
