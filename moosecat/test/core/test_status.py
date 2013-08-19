import moosecat.core as m
import unittest
import math


status_response = '''volume: -1
repeat: 1
random: 0
single: 0
consume: 0
playlist: 865
playlistlength: 16760
xfade: 0
mixrampdb: 0.000000
mixrampdelay: nan
state: play
song: 9915
songid: 9915
time: 72:200
elapsed: 71.877
bitrate: 256
audio: 44100:24:2
nextsong: 9916
nextsongid: 9916'''


class TestStatus(unittest.TestCase):
    def setUp(self):
        self.status = m.Status()
        self.status.test_begin()
        for line in status_response.splitlines():
            name, value = line.split(':', maxsplit=1)
            self.status.test_feed(name.strip(), value.strip())

    def test_attrs(self):
        self.assertTrue(self.status.volume == -1)
        self.assertTrue(self.status.repeat is True)
        self.assertTrue(self.status.random is False)
        self.assertTrue(self.status.single is False)
        self.assertTrue(self.status.consume is False)
        self.assertTrue(self.status.queue_length == 16760)
        self.assertTrue(self.status.queue_version == 865)
        self.assertTrue(self.status.state == m.Status.Playing)
        self.assertTrue(self.status.crossfade == 0)
        self.assertTrue(self.status.mixrampdb == 0.0)
        self.assertTrue(math.isnan(self.status.mixrampdelay))
        self.assertTrue(self.status.song_pos == 9915)
        self.assertTrue(self.status.song_id == 9915)
        self.assertTrue(self.status.next_song_id == 9916)
        self.assertTrue(self.status.next_song_pos == 9916)
        self.assertTrue(self.status.elapsed_seconds == 72)
        self.assertTrue(self.status.elapsed_ms == 71877)
        self.assertTrue(self.status.total_time == 200)
        self.assertTrue(self.status.is_updating == 0)

        # Audio stuff
        self.assertTrue(self.status.kbit_rate == 256)
        self.assertTrue(self.status.audio_sample_rate == 44100)
        self.assertTrue(self.status.audio_bits == 24)
        self.assertTrue(self.status.audio_channels == 2)

    def test_emptyAttributes(self):
        empty = m.Status()
        self.assertRaises(ValueError, lambda: empty.volume)
        self.assertRaises(ValueError, lambda: empty.repeat)
        self.assertRaises(ValueError, lambda: empty.random)
        self.assertRaises(ValueError, lambda: empty.single)
        self.assertRaises(ValueError, lambda: empty.consume)
        self.assertRaises(ValueError, lambda: empty.queue_length)
        self.assertRaises(ValueError, lambda: empty.queue_version)
        self.assertRaises(ValueError, lambda: empty.state)
        self.assertRaises(ValueError, lambda: empty.crossfade)
        self.assertRaises(ValueError, lambda: empty.mixrampdb)
        self.assertRaises(ValueError, lambda: empty.mixrampdelay)
        self.assertRaises(ValueError, lambda: empty.song_pos)
        self.assertRaises(ValueError, lambda: empty.song_id)
        self.assertRaises(ValueError, lambda: empty.next_song_id)
        self.assertRaises(ValueError, lambda: empty.next_song_pos)
        self.assertRaises(ValueError, lambda: empty.elapsed_seconds)
        self.assertRaises(ValueError, lambda: empty.elapsed_ms)
        self.assertRaises(ValueError, lambda: empty.total_time)
        self.assertRaises(ValueError, lambda: empty.is_updating)

        self.assertRaises(ValueError, lambda: empty.kbit_rate)
        self.assertRaises(ValueError, lambda: empty.audio_sample_rate)
        self.assertRaises(ValueError, lambda: empty.audio_bits)
        self.assertRaises(ValueError, lambda: empty.audio_channels)

    def tearDown(self):
        self.status = None

    def test_lockingWithNoConnection(self):
        with m.Client().lock_status() as status:
            self.assertTrue(status is None)

    def test_lockingWithConnection(self):
        from moosecat.test.mpd_test import MpdTestProcess
        test_mpd = MpdTestProcess()
        test_mpd.start()

        try:
            cl = m.Client()
            cl.connect(port=6666)
            cl.block_till_sync()
            self.assertTrue(cl.is_connected)

            with cl.lock_status() as status:
                self.assertTrue(status is not None)
                self.assertTrue(status.replay_gain_mode == 'off')
                self.assertTrue(status.audio_bits == 0)  # Nothing playing

        finally:
            test_mpd.stop()
            test_mpd.wait()
