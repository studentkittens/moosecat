import moosecat.core as m
import moosecat.test.mpd_test as mpd_test
import unittest


class TestClient(unittest.TestCase):
    def setUp(self):
        self.client = m.Client()
        self.client.connect(host='localhost', port=6666, timeout_sec=1)
        self.client.queue_add('/')

    def tearDown(self):
        self.client = None
        #self._mpd.stop()

    def test_outputs(self):
        def _check_outputs(check=True, set_to=False):
            with self.client.lock_outputs() as outputs:
                for output in outputs:
                    self.assertEqual(output.name, 'YouHopefullyDontUseThisDoYou')
                    self.assertEqual(output.enabled, check)

                    # Set the output to False

        print('one')
        _check_outputs(check=True, set_to=False)

        # self.client.raw_send('output_switch YouHopefullyDontUseThisDoYou 0')
        self.client.player_next()
        self.client.raw_send('setvol 0\n')
        # Sync with MPD
        print('sync')
        self.client.sync(m.Idle.OUTPUT)
        self.client.block_till_sync()

        print('two')
        _check_outputs(check=True, set_to=None)




        #outputs = self.client.outputs
        #self.assertEqual(len(outputs), 1)
        #self.assertEqual(outputs[0].name, 'YouHopefullyDontUseThisDoYou')
        #self.assertEqual(outputs[0].oid, 0)
        #self.assertEqual(outputs[0].enabled, True)
        #print('end')

        #outputs[0].enabled = False

        ## this is needed in order to update the metadata now
        ## (not after some mainloop iterations)
        #self.client.sync(m.Idle.OUTPUT)
        #outputs = self.client.outputs

        #self.assertEqual(outputs[0].enabled, False)

    #def test_command_list_mode(self):
    #    with self.client.command_list_mode():
    #        pass

    #    self.client.sync()
    #    self.assertTrue(self.client.is_connected)

    #    def test_ex():
    #        with self.client.command_list_mode():
    #            raise Exception("Well.")

    #    self.assertRaises(Exception, test_ex)

    #    self.client.sync()
    #    self.assertTrue(self.client.is_connected)

    #    # Make mpd playing, but paused
    #    self.client.player_play()
    #    self.client.player_pause()
    #    self.client.sync()
    #    self.assertTrue(self.client.status.state == m.Status.Paused)

    #    # Send a range of commands
    #    with self.client.command_list_mode():
    #        self.client.player_pause() # play
    #        self.client.player_pause() # pause
    #        self.client.player_pause() # play
    #        self.client.player_pause() # pause

    #    # check
    #    self.client.sync()
    #    self.assertTrue(self.client.status.state == m.Status.Paused)

    #def test_connect(self):
    #    print('waht?')
    #    self.assertTrue(self.client.is_connected)
    #    print('waht!')

if __name__ == '__main__':
    unittest.main()
