import moosecat.core.moose as m
import unittest


class TestStatus(unittest.TestCase):
    def setUp(self):
        self.client = m.Client()
        self.client.connect()

    def test_connect(self):
        self.assertTrue(self.client.is_connected)
        for i in range(10000):
            self.client.disconnect()
            self.assertTrue(self.client.is_connected is False)
            self.client.connect()
            self.assertTrue(self.client.is_connected)

    def tearDown(self):
        self.client = None
