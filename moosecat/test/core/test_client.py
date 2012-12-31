import moosecat.core as m
import moosecat.test.fakempd as fakempd
import unittest
import time


class TestClient(unittest.TestCase):
    def setUp(self):
        #self.shadow = fakempd.ShadowServer()
        #self.shadow.start()

        self.client = m.Client()
        print('pre-connect')
        print(self.client.connect(host='localhost', port=6666, timeout_sec=5))
        print('post-connect')

    def test_connect(self):
        print('test begin')
        self.assertTrue(self.client.is_connected)
        print('test assert')
        for i in range(10):
            self.client.disconnect()
            self.assertTrue(self.client.is_connected is False)
            self.client.connect()
            self.assertTrue(self.client.is_connected)

    def tearDown(self):
        print('Tearing down')
        self.client = None
        #self.shadow.stop()
        self.shadow = None

if __name__ == '__main__':
    unittest.main()
