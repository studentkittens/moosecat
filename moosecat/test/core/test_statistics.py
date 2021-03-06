import moosecat.core as m
import unittest
import time


stats_response = '''artists: 644
albums: 1326
songs: 16760
uptime: 407526
playtime: 285000
db_playtime: 4609008
db_update: 134909661
'''


class TestStats(unittest.TestCase):
    def setUp(self):
        self.stats = m.Statistics()
        self.stats.test_begin()
        for line in stats_response.splitlines():
            self.stats.test_feed(*line.split(':'))

    def test_attributes(self):
        self.assertTrue(self.stats.number_of_artists == 644)
        self.assertTrue(self.stats.number_of_albums == 1326)
        self.assertTrue(self.stats.number_of_songs == 16760)
        self.assertTrue(self.stats.uptime == time.gmtime(407526))
        self.assertTrue(self.stats.playtime == time.gmtime(285000))
        self.assertTrue(self.stats.db_update_time == time.gmtime(134909661))
        self.assertTrue(self.stats.db_playtime == time.gmtime(4609008))

    def test_emptyAttributes(self):
        empty = m.Statistics()
        self.assertRaises(ValueError, lambda: empty.number_of_artists)
        self.assertRaises(ValueError, lambda: empty.number_of_albums)
        self.assertRaises(ValueError, lambda: empty.number_of_songs)
        self.assertRaises(ValueError, lambda: empty.uptime)
        self.assertRaises(ValueError, lambda: empty.playtime)
        self.assertRaises(ValueError, lambda: empty.db_update_time)
        self.assertRaises(ValueError, lambda: empty.db_playtime)

    def test_lockingWithConnection(self):
        from moosecat.test.mpd_test import MpdTestProcess
        test_mpd = MpdTestProcess()
        test_mpd.start()

        try:
            cl = m.Client()
            cl.connect(port=6666)
            cl.player_play()
            cl.block_till_sync()
            self.assertTrue(cl.is_connected)

            with cl.lock_statistics() as stats:
                self.assertTrue(stats is not None)
                self.assertTrue(stats.number_of_artists == 1)
                self.assertTrue(stats.number_of_albums == 3)
                self.assertTrue(stats.number_of_songs == 36)
        finally:
            test_mpd.stop()
            test_mpd.wait()

    def tearDown(self):
        self.stats = None
