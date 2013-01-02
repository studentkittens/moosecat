import moosecat.core as m
import moosecat.test.fakempd as fakempd
import unittest


class TestClient(unittest.TestCase):
    def setUp(self):
        # TODO: this does not work well.
        #self.shadow = fakempd.ShadowServer()
        #self.shadow.start()

        self.client = m.Client()
        print(self.client.connect(host='localhost', port=6666, timeout_sec=5))

    def test_outputs(self):
        outputs = self.client.outputs

        # print('Le Outputs', outputs)
        # for output in outputs:
        #    print(output.oid, output.enabled, output.name, sep='\t')

        self.assertEqual(outputs[0].name, 'AlsaOutput')
        self.assertEqual(outputs[0].oid, 0)
        self.assertEqual(outputs[0].enabled, True)

        self.assertEqual(outputs[1].name, 'HTTPOutput')
        self.assertEqual(outputs[1].oid, 1)
        self.assertEqual(outputs[1].enabled, False)

    def test_connect(self):
        self.assertTrue(self.client.is_connected)
        for i in range(10):
            self.client.disconnect()
            self.assertTrue(self.client.is_connected is False)
            self.client.connect()
            self.assertTrue(self.client.is_connected)

    def tearDown(self):
        self.client = None
        #self.shadow.stop()
        #self.shadow = None

if __name__ == '__main__':
    unittest.main()
