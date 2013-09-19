#!/usr/bin/env python
# encoding: utf-8

'''
This is a framework to start a mpd instance on a specified port, with some
pre-defined songs and a pre-defined (minimal as possible) config.

It is similar to QBall's AutoMPD, but simpler and only used for testing for now.
'''

import os
import sys
import json
import time
import shutil
import telnetlib
import subprocess
import socket


# Import the tagging library
try:
    import stagger
except ImportError:
    print('Install stagger with: sudo pip install stagger')
    sys.exit(-1)


CONFIG_NAME = 'mpd.config'
CONFIG_TEMPLATE = '''
# set directories (auto-generated)
music_directory "{music_dir}"
playlist_directory "{playlist_dir}"
db_file "{top_dir}/mpd.db"
log_file "{top_dir}/mpd.log"
pid_file "{top_dir}/mpd.pid"
state_file "{top_dir}/mpd.state"

audio_output {{
        type "fifo"
        name "YouHopefullyDontUseThisDoYou"
        path "{top_dir}/mpd.fifo"
}}

# should run on different port than normal mpd
port "6666"
'''

SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))


def touch(top_dir, name):
    'create an empty named <name> under <top_dir>'
    full_path = os.path.join(top_dir, name)
    if not os.path.exists(full_path):
        open(full_path, 'w').close()

    return full_path


def mkdir(top_dir, name):
    'create an empty directory named <name> under <top_dir>'
    full_path = os.path.join(top_dir, name)
    if not os.path.exists(full_path):
        os.mkdir(full_path)

    return full_path


def create_config(top_dir, music_dir, playlist_dir):
    '''
    Fill the CONFIG_TEMPLATE and write it under the top_dir

    :returns: the path to the config-file
    '''
    full_path = os.path.join(top_dir, CONFIG_NAME)
    with open(full_path, 'w') as f:
        f.write(CONFIG_TEMPLATE.format(
                music_dir=music_dir,
                playlist_dir=playlist_dir,
                top_dir=top_dir
            )
        )

    return full_path


def create_music_directory(music_dir, database):
    '''
    Create a mp3 file database in <music_dir>

    An empty mp3 file is used as holder for different tags.

    :database: a list of dictionaries containing appropiate tags.
    '''
    mp3_file = os.path.join(SCRIPT_PATH, 'point1sec.mp3')
    for song in database:
        song_file = stagger.read_tag(mp3_file)

        # Set some tags on the file
        song_file.artist = song.get('artist')
        song_file.album = song.get('album')
        song_file.title = song.get('title')
        song_file.album_artist = song.get('album_artist')
        song_file.composer = song.get('composer')
        song_file.track = song.get('track')

        # Write a well named file into the test database
        new_filename = os.path.join(music_dir,
                    '_'.join([
                            song_file.artist,
                            song_file.album,
                            song_file.title]
                    ) + '.mp3')

        # Apparently stagger needs another mp3 file to write one.
        # Luckily we have one.
        shutil.copyfile(mp3_file, new_filename)
        song_file.write(new_filename)


def remove_dirstruct(root_path='/tmp'):
    full_path = os.path.join(root_path, '.test-mpd')
    shutil.rmtree(full_path)


def create_dirstruct(root_path='/tmp'):
    '''
    Create a directory under root_path containing:

        /tmp/
            .test-mpd/
                    mpd.conf
                    mpd.db
                    mpd.log
                    mpd.pid
                    mpd.state
                    music-files/
                        artist_album_title.mp3
                        ...
                    playlists/

        :returns: the path to .test-mpd-<uuid>
    '''
    try:
        full_path = os.path.join(root_path, '.test-mpd')
        if os.path.exists(full_path):
            remove_dirstruct(root_path)

        # create the top directory
        top_dir = mkdir(root_path, '.test-mpd')

        # create the inner directories
        playlist_dir = mkdir(top_dir, 'playlists')
        music_dir = mkdir(top_dir, 'music-files')

        # Create empty files as mpd demands
        for to_touch in ['mpd.db', 'mpd.log', 'mpd.pid', 'mpd.state']:
            touch(top_dir, to_touch)

        # fill the config with the right stuff
        create_config(top_dir, music_dir, playlist_dir)

        # Load the tags from urlaub.json.db
        with open(os.path.join(SCRIPT_PATH, 'urlaub.json.db'), 'r') as f:
            json_database = json.load(f)

        # fill some tagged mp3 in there
        create_music_directory(music_dir, json_database)

    except FileNotFoundError as err:
        print('Cannot create test-mpd directory:', err)
    else:
        return top_dir


class MpdTestProcess:
    def __init__(self, root='/tmp', do_print=False):
        self._top_dir = None
        self._root = root
        self._pid = 0
        self._print = do_print

    def _spawn(self):
        command = 'mpd --no-daemon --stdout --verbose {conf_path}'.format(
                conf_path=os.path.join(self._top_dir, CONFIG_NAME)
        )

        if self._print:
            self._pipe = subprocess.Popen(
                    command, shell=True
            )
            print('-- Spawned Server Process.')
        else:
            self._pipe = subprocess.Popen(
                    command, shell=True,
                    stderr=subprocess.PIPE,
                    stdout=subprocess.PIPE
            )

        if self._print:
            print('-- Server should run now.. Configuring it now: ', end='')

        # Silly hack, apparently mpd takes about a second till it accepts
        # connections
        time.sleep(1)

        INIT = '''\
            mpc -p 6666 add /
            mpc -p 6666 save test1
            mpc -p 6666 save test2
        '''

        for line in INIT.splitlines():
            subprocess.call(line, shell=True)

        if self._print:
            print('[DONE]')

    def start(self):
        self._top_dir = create_dirstruct(root_path=self._root)
        self._spawn()

    def wait(self):
        self._pipe.wait()

    def stop(self):
        if os.path.exists(self._top_dir):
            shutil.rmtree(self._top_dir)

        if self._pipe.returncode is None:
            self._pipe.terminate()


if __name__ == '__main__':
    import time
    m = MpdTestProcess(do_print=True)
    m.start()

    try:
        m.wait()
    except KeyboardInterrupt:
        pass
    finally:
        m.stop()
