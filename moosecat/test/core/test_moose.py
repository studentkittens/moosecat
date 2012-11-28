import moosecat.core.moose as m
import unittest
import time
import math


class TestSong(unittest.TestCase):
    def setUp(self):
        self.song = m.Song()
        self.song.test_begin()

    def tearDown(self):
        self.song = None

    def test_emptyAttrs(self):
        with self.assertRaises(ValueError):
            print(m.Song().artist)

    def test_setEmptyAttr(self):
        with self.assertRaises(ValueError):
            m.Song().queue_pos = 42

    def test_allProps(self):
        empty_song = m.Song()
        for attr in empty_song.__class__.__dict__.keys():
            str_attr = str(attr)
            if str_attr[0] != '_' and str_attr[:4] != 'test':
                with self.assertRaises(ValueError):
                    eval('empty_song.' + str_attr)

    def test_BeginDummy(self):
        self.song.test_feed('Artist', 'test')
        self.assertTrue(self.song.artist == 'test')
        self.assertTrue(self.song.album == '')

    def test_nonTagProps(self):
        # Is set to something by test_begin()
        self.assertTrue(self.song.uri != '')

        # See if we have an actual struct_time
        self.assertTrue(self.song.last_modified, time.struct_time)

        self.assertTrue(self.song.queue_pos == 0)
        self.assertTrue(self.song.queue_id == 0)
        self.assertTrue(self.song.duration == 0)

        self.song.queue_pos = 42
        self.assertTrue(self.song.queue_pos == 42)


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
        self.assertTrue(self.stats.uptime == 407526)
        self.assertTrue(self.stats.playtime == 285000)
        self.assertTrue(self.stats.db_update_time == 134909661)
        self.assertTrue(self.stats.db_playtime == 4609008)

    def test_emptyAttributes(self):
        empty = m.Statistics()
        self.assertRaises(ValueError, lambda: empty.number_of_artists)
        self.assertRaises(ValueError, lambda: empty.number_of_albums)
        self.assertRaises(ValueError, lambda: empty.number_of_songs)
        self.assertRaises(ValueError, lambda: empty.uptime)
        self.assertRaises(ValueError, lambda: empty.playtime)
        self.assertRaises(ValueError, lambda: empty.db_update_time)
        self.assertRaises(ValueError, lambda: empty.db_playtime)

    def tearDown(self):
        self.stats = None


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
        self.assertTrue(self.status.update_id == 0)

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
        self.assertRaises(ValueError, lambda: empty.update_id)

        self.assertRaises(ValueError, lambda: empty.kbit_rate)
        self.assertRaises(ValueError, lambda: empty.audio_sample_rate)
        self.assertRaises(ValueError, lambda: empty.audio_bits)
        self.assertRaises(ValueError, lambda: empty.audio_channels)


    def tearDown(self):
        self.status = None
