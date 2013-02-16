import moosecat.core as m
import moosecat.test.mpd_test as mpd_test
import unittest


class TestClient(unittest.TestCase):
    def setUp(self):
        self._mpd = mpd_test.MpdTestProcess()
        self._mpd.start()

        self.client = m.Client()
        self.client.connect(host='localhost', port=6666, timeout_sec=5)

    def test_outputs(self):
        outputs = self.client.outputs

        self.assertEqual(len(outputs), 1)
        self.assertEqual(outputs[0].name, 'YouHopefullyDontUseThisDoYou')
        self.assertEqual(outputs[0].oid, 0)
        self.assertEqual(outputs[0].enabled, True)

        outputs[0].enabled = False

        # this is needed in order to update the metadata now
        # (not after some mainloop iterations)
        self.client.force_update_metadata(m.Idle.OUTPUT)
        outputs = self.client.outputs

        self.assertEqual(outputs[0].enabled, False)

    def test_connect(self):
        self.assertTrue(self.client.is_connected)
        for i in range(10):
            self.client.disconnect()
            self.assertTrue(self.client.is_connected is False)
            self.client.connect()
            self.assertTrue(self.client.is_connected)

    def tearDown(self):
        self.client = None
        self._mpd.stop()

if __name__ == '__main__':
    unittest.main()
