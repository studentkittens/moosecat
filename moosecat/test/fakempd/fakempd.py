#!/usr/bin/env python
# encoding: utf-8

'''
###########################################################################
#                FakeMPD - A silly idea brought to an end                 #
###########################################################################

This is a SocketServer that acts like MPD. It only impl
I'm aware that there is a Ruby Version of this at librmpd.
It seems to implement an old version of MPD and I don't know much Ruby,
so

Advantages:

    * Makes testing a lot easier/possible.
    * Can act as dummy server when no real server is there.
    * It only depend on Standard Python.
    * Later version might even announce via Avahi.

Disadvantages:

    * I spent way too much time on this.
    * This is a dependency of a test, which has it's own set of tests.
      Im fucking crazy.

Test it like this:

    $ python fake_mpd.py
    $ telnet localhost 6666
    status
    ...
    OK
    ^]
    telnet>^D
    $

Start it from

Author:

    Christopher Pahl 2012-2013
'''

import os
import time
import json
import errno
import traceback
import random
import threading
import signal

import socketserver as serv
import socket

from select import select
from itertools import chain
from functools import partial


###########################################################################
#                          Globals                                        #
###########################################################################
'''
add addid clear clearerror
consume count crossfade currentsong delete deleteid disableoutput
enableoutput find findadd list listall listallinfo
listplaylist listplaylistinfo listplaylists load lsinfo mixrampdb mixrampdelay move
moveid next outputs pause play
playid playlist playlistadd playlistclear playlistdelete playlistfind playlistid playlistinfo
playlistmove playlistsearch plchanges plchangesposid previous prio prioid random
rename repeat replay_gain_mode replay_gain_status rescan rm save
search searchadd searchaddpl seek seekcur seekid setvol
shuffle single stats status stop swap swapid
update
'''

MPD_VERSION = b'0.17.0'

REQUST_HANDLER_LOCK = threading.Lock()

IDLE_ALLOWED_EVENTS = ['database', 'stored_playlist', 'queue', 'player', 'mixer',
                       'output', 'options', 'update', 'sticker', 'subscription'
                       'sticker']

# Commands that are not implemented and just get ACK'd
NOT_IMPLEMENTED = ['channels', 'config',
                   'decoders', 'notcommands', 'password', 'ping',
                   'readmessages', 'sendmessage', 'subscribe', 'tagtypes',
                   'unsubscripe', 'urlhandlers']

# The Protocol Spec is stored in here.
# It is defined automatically by the requst_handler decorator.
PROTOCOL = {}

###########################################################################
#                               Decorators                                #
###########################################################################


def request_handler(min_args=0, max_args=0, aliases=[], events=[]):
    def _request_handler(function):
        # Let's check if needed conventions are fulfilled
        assert function.__name__[:8] == 'respond_', 'Function does not look like respond_<name>'

        def __request_handler(gstate, lstate):
            res = function(gstate, lstate)
            if 'queue' in events:
                gstate.status.playlist += 1

            if lstate._skip_idle:
                lstate._skip_idle = False
            else:
                if len(events) is not 0:
                    gstate.share_event(*events)
            return res

        for func_name in chain([function.__name__[8:]], aliases):
            PROTOCOL[func_name] = (min_args, max_args, __request_handler)
        return function
    return _request_handler

###########################################################################
#                           Response Functions                            #
###########################################################################

########################
#  Reflection Handler  #
########################


@request_handler()
def respond_commands(gstate, lstate):
    cmnd_list = []
    for key in PROTOCOL.keys():
        cmnd_list.append('command: ' + key)
    return '\n'.join(cmnd_list)


####################
#  Status Handler  #
####################

@request_handler()
def respond_clearerror(gstate, lstate):
    gstate.status.error = False


@request_handler()
def respond_status(gstate, lstate):
    return gstate.status


@request_handler()
def respond_stats(gstate, lstate):
    return gstate.stats


@request_handler()
def respond_currentsong(gstate, lstate):
    return gstate.currentsong


####################
#  Option Handler  #
####################

def _option_converter(old, lstate, string):
    try:
        num = min(1, max(0, int(string)))
        if num == old:
            lstate.skip_sideeffect()
        return num
    except ValueError:
        raise ProtocolError(lstate, 'Boolean (0/1) expected: {}'.format(string))


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_consume(gstate, lstate):
    gstate.status.consume = _option_converter(gstate.status.consume, lstate, lstate.args[0])


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_crossfade(gstate, lstate):
    try:
        gstate.status.crossfade = int(lstate.args[0])
    except ValueError:
        raise ProtocolError(lstate, 'Integer expected: ' + lstate.args[0], Error.ARGUMENT)


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_random(gstate, lstate):
    gstate.status.random = _option_converter(gstate.status.random, lstate, lstate.args[0])


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_single(gstate, lstate):
    gstate.status.single = _option_converter(gstate.status.single, lstate, lstate.args[0])


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_repeat(gstate, lstate):
    gstate.status.repeat = _option_converter(gstate.status.repeat, lstate, lstate.args[0])


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_mixrampdb(gstate, lstate):
    try:
        gstate.status.mixrampdb = float(lstate.args[0])
    except ValueError:
        raise ProtocolError(lstate, 'Float expected', Error.ARGUMENT)


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_mixrampdelay(gstate, lstate):
    try:
        gstate.status.mixrampdelay = float(lstate.args[0])
    except ValueError:
        raise ProtocolError(lstate, 'Float expected', Error.ARGUMENT)


@request_handler(min_args=1, max_args=1, events=['options'])
def respond_replay_gain_mode(gstate, lstate):
    arg = lstate.args[0]
    if isinstance(arg, bytes) and arg.decode('utf-8') in ['album', 'track', 'off', 'auto']:
        gstate.replay_gain_status = arg
    else:
        raise ProtocolError(lstate, 'Unrecognized replay gain mode', Error.ARGUMENT)


@request_handler()
def respond_replay_gain_status(gstate, lstate):
    return 'replay_gain_mode: {}'.format(gstate.replay_gain_status)


###################
#  Mixer Handler  #
###################

@request_handler(min_args=1, max_args=1, events=['mixer'])
def respond_setvol(gstate, lstate):
    gstate.status.volume = lstate.args[0]

###################
#  Queue Handler  #
###################


@request_handler(min_args=1, max_args=1, events=['queue'])
def respond_delete(gstate, lstate):
    del gstate.queue[lstate.args[0]]


@request_handler(events=['queue'])
def respond_clear(gstate, lstate):
    gstate.queue = Playlist()


@request_handler(min_args=1, max_args=1, events=['queue'])
def respond_add(gstate, lstate):
    gstate.queue.append(lstate.args[0])


######################
#  Database Handler  #
######################


@request_handler(min_args=0, max_args=1)
def respond_listall(gstate, lstate):
    return '\n'.join([str(song) for song in gstate.queue])

####################
#  Output Handler  #
####################


@request_handler()
def respond_outputs(gstate, lstate):
    outputs = []
    for idx, output in enumerate(gstate.outputs):
        outputs.append('outputid: {oid}\noutputname: {oname}\noutputenabled: {oen}'.format(
            oid=idx,
            oname=output[0],
            oen=output[1]))

    return '\n'.join(outputs)


def _changeoutput(gstate, lstate, state=1):
    try:
        if gstate.outputs[lstate.args[0]][1] == state:
            lstate.skip_sideeffect()
        else:
            gstate.outputs[lstate.args[0]][1] = state
    except IndexError:
        raise ProtocolError(lstate, 'No such audio output', Error.SERVER)


@request_handler(min_args=1, max_args=1, events=['output'])
def respond_enableoutput(gstate, lstate, state=1):
    return _changeoutput(gstate, lstate, state=1)


@request_handler(min_args=1, max_args=1, events=['output'])
def respond_disableoutput(gstate, lstate):
    return _changeoutput(gstate, lstate, state=0)

######################
#  Playback Handler  #
######################


@request_handler(aliases=['previous'], events=['player'])
def respond_next(gstate, lstate):
    # Totally faked
    next_pos = random.randrange(1, gstate.status.playlistlength)
    gstate.status.song = next_pos
    gstate.status.song = gstate.status.playlistlength - next_pos


def _state_helper(gstate, lstate, new_state):
    if gstate.status.state == new_state:
        lstate.skip_sideeffect()
    else:
        gstate.status.state = new_state


@request_handler(min_args=0, max_args=1, events=['player'])
def respond_pause(gstate, lstate):
    if len(lstate.args) is 0:
        if gstate.status.state == Status.STATE_PLAY:
            new_state = Status.STATE_PAUSE
        else:
            new_state = Status.STATE_PLAY
    else:
        if lstate.args[0] == '0':
            new_state = Status.STATE_PLAY
        else:
            new_state = Status.STATE_PAUSE

    _state_helper(gstate, lstate, new_state)


@request_handler(min_args=0, max_args=1, aliases=['playid'], events=['player'])
def respond_play(gstate, lstate):
    _state_helper(gstate, lstate, Status.STATE_PLAY)
    if len(lstate.args) > 0:
        # Fake skipping to some song
        respond_next(gstate, lstate)


@request_handler(events=['player'])
def respond_stop(gstate, lstate):
    _state_helper(gstate, lstate, Status.STATE_STOP)


@request_handler(min_args=2, max_args=2, aliases=['seekid'], events=['player'])
def respond_seek(gstate, lstate):
    # Fake switching to a new pos/id
    respond_next(gstate, lstate)

    gstate.status.time = lstate.args[1]
    gstate.status_elapsed = lstate.args[1]


@request_handler(min_args=1, max_args=1, events=['player'])
def respond_seekcur(gstate, lstate):
    gstate.status.time = lstate.args[1]
    gstate.status_elapsed = lstate.args[1]


####################
#  Debug Responds  #
####################

@request_handler()
def respond__trigger_idle(gstate, lstate):
    gstate.share_event('options', 'mixer')


#############################
#  Stored Playlist Handler  #
#############################

@request_handler()
def respond_listplaylists(gstate, lstate):
    resp = []
    for playlist in gstate.playlists:
        resp.append('playlist: {name}\nLast-Modified: {lm}'.format(
            name=playlist.name,
            lm=playlist.last_modified
        ))

    return '\n'.join(resp)


###########################################################################
#                              Utils                                      #
###########################################################################


def format_mpd_time(seconds):
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(seconds))


def keep_or_convert_to_int(string):
    try:
        return int(string)
    except ValueError:
        return string


class AttributesToStringMixin:
    '''
    Overloads __str__ by collecting all members,
    and packing in a "key: value" fashion.
    '''
    def __str__(self):
        pairs = []
        for attr, value in self.__dict__.items():
            if not attr.startswith('_'):
                pairs.append(': '.join([attr.replace('_', '-'), str(value)]))
        return '\n'.join(pairs)


def keep_or_encode_to_str(string):
    if isinstance(string, bytes):
        return string.decode('utf-8')
    else:
        return string


def get_local_file_path(filename):
    return os.path.join(os.path.dirname(__file__), filename)


###########################################################################
#                                 Errors                                  #
###########################################################################


class Error:
    'Error Enum from libmpdclient/errors.h'
    SUCCESS = 0
    OOM = 1
    ARGUMENT = 2
    STATE = 3
    TIMEOUT = 4
    SYSTEM = 5
    RESOLVER = 6
    MALFORMED = 7
    CLOSED = 8
    SERVER = 9


class ProtocolError(Exception):
    '''
    An Exception that can be thrown inside a request_handler.
    It can be converted to a string, that is suitable for writing it
    back to the client (e.g. ACK [5@0] {status} Im dumb\n)

    Just use this:

        raise ProtocolError(local_state, 'Im dumb', Error.SERVER)
    '''
    def __init__(self, state, cause, errid=Error.SERVER):
        self._state = state
        self._cause = cause
        self._errid = errid
        Exception.__init__(self)

    def __str__(self):
        return 'ACK [{error_id}@{cmnd_list_num}] {{{current_cmd}}} {cause}\n'.format(
            error_id=self._errid,
            cmnd_list_num=self._state.command_list_idx,
            current_cmd=self._state.current_command,
            cause=self._cause
        )


class CloseConnection(Exception):
    '''
    Exceptions thrown on close/kill/fatal errors.
    It causes the Handler Loop to break out.
    '''
    pass


###########################################################################
#                              State Classes                              #
###########################################################################


class LocalState:
    '''
    All State Objects are bundled here.
    '''
    def __init__(self):
        # Arguments passed to a command
        self.args = tuple()

        # A command list was issued?
        self.command_list = False
        self.command_list_idx = 0

        # Currently processed Command
        self.current_command = ''

        # An Error happened?
        self.error = False

        # This is based on: http://tinyurl.com/d63z7nt
        self.rpipe, self.wpipe = os.pipe()

        # the writable file of this connection
        self._event_set = set()

        # a response handler might set out a sideffect with this.
        self._skip_idle = False

    def skip_sideeffect(self):
        self._skip_idle = True

    def write_event(self, event):
        if event.decode('utf-8') in IDLE_ALLOWED_EVENTS:
            self._event_set.add(event)
        else:
            print('Warning:', event, 'is not a valid event!')

    def finish_events(self):
        # It's unimportant what we send, just needs to be a single char
        os.write(self.wpipe, b'!')

    def get_events(self):
        rc = list(self._event_set)
        self._event_set.clear()
        return rc


class GlobalState:
    '''
    State representing the state of the server. I.e. things that
    are the same for all connections.
    '''
    def __init__(self):
        # State Data (Default)
        self.status = Status()
        self.currentsong = Song()
        self.stats = Stats()

        # Replay Gain.. this is for some reason not in Status?
        self.replay_gain_status = 'off'

        # Playlists / Database
        self.queue = Playlist()
        self.db = Playlist.from_json(get_local_file_path('urlaub.db'))

        # Audio Output (fake one HTTP and one ALSA output)
        self.outputs = [['AlsaOutput', 1], ['HTTPOutput', 0]]

        # A list of stored playlists
        self.playlists = [Playlist(name='Favourites'), Playlist(name='oldstuff')]

        # A list of local states around.
        # This is necessary for sharing events on all connections.
        self._local_states = []

    def __iter__(self):
        return iter(self.queue)

    def append_state(self, lstate):
        self._local_states.append(lstate)

    def remove_state(self, lstate):
        try:
            self._local_states.remove(lstate)
        except ValueError:
            pass

    def share_event(self, *args):
        '''
        Goes through all local states of the server and distributes the passed events.
        '''
        for lstate in self._local_states:
            for event in args:
                byte_event = event
                if not isinstance(event, bytes):
                    byte_event = event.encode('utf-8')
                # Write Event does no actual write.
                # It stores stuff in a set()
                lstate.write_event(byte_event)

            # Every LocalState has a Pipe, that gets written to now.
            # If a connection waits in idle, the pipe has readable data,
            # and the connection wakes up from select().
            lstate.finish_events()

###########################################################################
#                                Substates                                #
###########################################################################


class Song(AttributesToStringMixin):
    def __init__(self):
        self.file = 'file://dummy'
        self.Last_Modifed = '2011-04-02T22:01:36Z'
        self.Time = 168
        self.Artist = '<empty>'
        self.Album = '<empty>'
        self.AlbumArtist = '<empty>'
        self.Title = '<empty>'
        self.Track = '0/0'
        self.Genre = 'Misc'
        self.Composer = '<empt>'
        self.Disc = '1/1'
        self.Pos = 0
        self.Id = 0
        # Musibrainz Tags omitted.

    @classmethod
    def from_dict(cls, song_dict):
        instance = Song()
        for key, value in song_dict.items():
            instance.__dict__[key.replace('-', '_')] = value
        return instance


class Status(AttributesToStringMixin):
    'Models the Status Procotol Command (with Defaults)'
    STATE_PLAY = 'play'
    STATE_PAUSE = 'pause'
    STATE_STOP = 'stop'
    STATE_UNKNOWN = 'unknown'

    def __init__(self):
        self.repeat = 0
        self.random = 0
        self.single = 0
        self.consume = 0

        self.playlist = 2
        self.playlistlength = 2

        self.xfade = 0
        self.mixrampdb = 0.0
        self.mixrampdelay = float('nan')
        self.state = Status.STATE_PLAY

        self.song = 1
        self.songid = 1

        self.time = '150:485'
        self.elapased = 149.885
        self.bitrate = 256
        self.audio = '44100:24:2'
        self.nextsong = 2
        self.nextsongid = 2


class Stats(AttributesToStringMixin):
    def __init__(self):
        self.artists = 1
        self.albums = 2
        self.uptime = 1000
        self.playtime = 1300
        self.db_playtime = 4609008
        self.db_update = 1000000

###########################################################################
#                          Database / Playlists                           #
###########################################################################


class Playlist:
    def __init__(self, name=None, songs=[]):
        self._songs = songs
        self._name = name
        self._last_modified = time.time()

    def append(self, song):
        self._songs.append(song)

    def __iter__(self):
        return iter(self._songs)

    def __getitem__(self, idx):
        return self._songs[idx]

    def __delitem__(self, idx):
        del self._songs[idx]

    @property
    def name(self):
        return self._name

    @property
    def last_modified(self):
        return format_mpd_time(self._last_modified)

    @classmethod
    def from_json(cls, path):
        instance = Playlist()
        with open(path, 'r') as f:
            for node in json.load(f):
                instance.append(Song.from_dict(node))
        return instance


###########################################################################
#                               Networking                                #
###########################################################################


class FakeMPDHandler(serv.StreamRequestHandler):
    '''
    Object that is instanciated once for each connection.
    It does the IO and servers as main-logic for pretty much everything.

    It gets implicitly created by FakeMPDServer.
    '''

    def _write(self, b_message):
        self._server_message('Writing:', b_message.splitlines()[0], out=True)
        self.wfile.write(b_message)

    def _respond(self, message, ack_with_ok=True):
        '''
        Do the writeback to the client in a way conforming to MPD.
        '''
        # Convert the incoming object to a string,
        # and then encode it as UTF-8 bytestring (needed for sockets)
        # Also checks if it alreay is a bytestring.
        if not isinstance(message, bytes):
            b_message = str(message).encode('utf-8')
        else:
            b_message = message

        # Check if an Error happened.
        if isinstance(message, ProtocolError):
            self._write(b_message)
        else:
            # Okay the message. If no output was written,
            # we just write the OK.
            if len(b_message) is not 0:
                # Write it back to the client
                self._write(b_message)

                # New line for OK...
                self._write(b'\n')

            if ack_with_ok is True:
                self._write(b'OK\n')

    def _lookup(self, cmnd):
        '''
        Looks up the command in the PROTOCOL dictionary.

        If there is a valid respond_func it will call it, and return a Response,
        or *return* a ProcotolError (not raising it). If the command does not provide
        enough, or too many arguments an error is alos thrown.

        If a command is unknown it silently get's OK'd (by returing an empty string)

        :cmnd: the command-line received from the client. Unprocessed.
        :returns: Something that can be passed to str(), or to FakeMPDHandler.respond()
        '''
        # We get the full command; e.g. 'plchanges 0 1 2 3'
        split = cmnd.split()
        if len(split) < 1:
            return ProtocolError(self.lstate, 'No command given', Error.ARGUMENT)

        cmnd_name = str(split[0], 'ascii')

        # See if we have a handler for this command
        min_args, max_args, respond_func = PROTOCOL.get(cmnd_name, [0, 0, None])
        if respond_func is not None:

            # Remember the current command for the duration
            # of a request. It's undefined afterwards.
            self.lstate.current_command = cmnd_name

            # See if the command has additional arguments.
            # If so we try to convert them to integers, or leave them as it is.
            # (Just makes stuff easier)
            if len(split) > 1:
                self.lstate.args = list(map(keep_or_convert_to_int, split[1:]))
            else:
                self.lstate.args = []

            # See if the number of arguments fits.
            # If so we *return* an Exception. Might be a weird, but this
            # special Exception can be converted easily to a 'ACK ...' string.
            if not (min_args <= len(self.lstate.args) <= max_args):
                return ProtocolError(self.lstate,
                                     'wrong number of arguments for "{}"'.format(cmnd_name),
                                     Error.ARGUMENT)

            # Try to run the Respond Func.
            # If it fails it might throw a ProcotolError with the reason.
            # The ProtocolError can be converted to a ready to use bytemessage,
            # describing the error on a Protocol level.
            try:
                # Can return arbitary python objects
                with REQUST_HANDLER_LOCK:
                    r = respond_func(self.server.gstate, self.lstate)

                if r is None:
                    return ''
                else:
                    return r
            except ProtocolError as err:
                # Gets converted to a str later
                return err
        else:
            # We do not have a handler for this one.
            # Therefore we lookup if we actually implemented it:
            if cmnd_name in NOT_IMPLEMENTED:
                # Just silently OK it and hope it doesn't do any harm.
                return ''
            else:
                return ProtocolError(self.lstate,
                                     'unknown command "{}"'.format(cmnd_name),
                                     Error.ARGUMENT)

    def _server_message(self, *args, **kwargs):
        arg_str = ' '.join([keep_or_encode_to_str(i) for i in args]).strip()

        if len(arg_str) is 0:
            return

        ellipsis = '...' if len(args) > 2 else ''
        client_name = ':'.join([str(e) for e in self.client_address])

        if 'out' in kwargs:
            direction = '<='
        else:
            direction = '=>'

        print('--', client_name, direction, arg_str, ellipsis)

    def _process_line(self, line):
        # Commands that need to implemented at Socket level,
        # i.e. no request_handler is called.
        SOCKET_LEVEL_COMMANDS = {
            'idle': FakeMPDHandler._handle_idle,
            'noidle': FakeMPDHandler._handle_noidle,
            'close': FakeMPDHandler._handle_close,
            'kill': FakeMPDHandler._handle_kill,
            'command_list_begin': partial(FakeMPDHandler._handle_command_list_begin, ack_with_list_ok=False),
            'command_list_ok_begin': partial(FakeMPDHandler._handle_command_list_begin, ack_with_list_ok=True),
            'command_list_end': FakeMPDHandler._handle_command_list_end
        }

        if len(line) == 0:
            # This is just a quirk of MPD's protocol (if nothing given)
            return ProtocolError(self.lstate, 'No command given', Error.ARGUMENT)
        else:
            command_name = line.split()[0].decode('utf-8')
            if command_name in SOCKET_LEVEL_COMMANDS:
                # Some commands need special treatment, and cannot be
                # with a normal request handler. (Since they cannot modify
                # the connection on a socket level)

                # Call the selected special-handler..
                SOCKET_LEVEL_COMMANDS[command_name](self)

                # return None to mark an already handled line
                return
            else:
                # Lookup a response function (,,process that shit!'')
                return self._lookup(line)

    ######################
    #  Special Handlers  #
    ######################

    def _handle_close(self):
        # This will cause the handler loop to break out.
        raise CloseConnection("FakeMPD: You issued ,,close'' - Goodbye, good Sir.")

    def _handle_noidle(self):
        # This is a bit strange. ''noidle'' alone, does not respond with a OK
        # or ACK, just plain nothing.
        pass

    def _handle_kill(self):
        # Difference to normal Protocol! kill does not actually the server.
        # It works just the same like close.
        # Any client out there using kill? Why?
        raise CloseConnection("FakeMPD: I survive ,,kill''!\n")

    def _handle_command_list_end(self):
        # Difference to MPD: MPD does not recognize command_list_end
        # as seperate command and throws an error.
        # We'll just be nice and handle it like noidle.
        pass

    def _handle_command_list_begin(self, ack_with_list_ok=False):
        # The commandlist pattern:
        #   command_list_[ok_]begin
        #   ... commands like status ...
        #   command_list_end
        #
        # What to do here:
        #   1) read a line
        #   2) if the line is not b'command_list_end':
        #          resp.append(process the line)
        #   3) else:
        #          end loop, print all cached responses
        responses = []

        close_exc = None
        success = True
        while True:
            line = self.rfile.readline().strip()
            if line == b'command_list_end':
                for resp in responses:
                    self._respond(resp, ack_with_ok=False)
                    if ack_with_list_ok:
                        self._write(b'list_OK\n')
                if success is True:
                    self._write(b'OK\n')
                break
            elif success is True:
                try:
                    resp = self._process_line(line)
                except CloseConnection as e:
                    close_exc = e
                    resp = None

                if resp is not None:
                    responses.append(resp)
                    if isinstance(resp, ProtocolError):
                        success = False

        if close_exc is not None:
            raise close_exc

    def _handle_idle(self):
        'Called if the command was "idle"'

        # Two things might happen here:
        #   1) Client sends 'noidle' -> Stop reading, don't idle.
        #   2) Client sends other stuff -> Die. (As requested by protocol)
        #   3) Some request handler in another connection might
        #      report an event, in that case, we'll stop handling idle,
        #      and print the list of events that happened.
        try:
            readable, _, _ = select([self.rfile, self.lstate.rpipe], [], [])

            # If events happened we need to unread everything from the pipe
            if self.lstate.rpipe in readable:
                os.read(self.lstate.rpipe, 1024)

                # Now get the happened events and write them out to the client
                for event in self.lstate.get_events():
                    self._write(b'changed: ' + event + b'\n')

            # Client sended something to us.
            if self.rfile in readable:
                resp = self.rfile.readline().strip()
                if resp != b'noidle':
                    raise CloseConnection("FakeMPD: Only ,,noidle'' allowed here\n")
                else:
                    self._server_message('got "noidle"')

            # OK the event list
            self._write(b'OK\n')
        except socket.error as e:
            self._server_message('FakeMPD:Connection-Error (idle mode):', e)
            traceback.print_exc()

    def _init(self):
        # Every connection has a "local state"
        self.lstate = LocalState()

        # ... and each server a "global state",
        # that needs to know about local states.
        with REQUST_HANDLER_LOCK:
            self.server.gstate.append_state(self.lstate)

    def _die(self):
        with REQUST_HANDLER_LOCK:
            self.server.gstate.remove_state(self.lstate)

        os.close(self.lstate.rpipe)
        os.close(self.lstate.wpipe)

    #############################
    #  RequestHandler Function  #
    #############################

    def handle(self):
        '''
        handle() is called once a connection is established on the Server.
        Once handle returns the connection is closed
        '''
        # get a state!
        self._init()

        # Send the greeting message as demanded
        self._write(b'OK MPD ' + MPD_VERSION + b'\n')
        self._server_message('Greeting: OK MPD', MPD_VERSION)

        # While the client does not quit, we try to read input from it.
        while True:
            try:
                # Read a single line
                cmnd = self.rfile.readline().strip()

                # Some useful debugging
                self._server_message('Receiving', str(cmnd))

                # Decide what to do with the line
                resp = self._process_line(cmnd)

                if resp is None:
                    continue

                # Write resp back to the client
                self._respond(resp)
            except socket.error as e:
                if e.errno == errno.EPIPE:
                    self._server_message('Client closed the connection.')
                else:
                    self._server_message('Connection Error: (%s)' % e)
                    traceback.print_exc()
                break
            except CloseConnection as c:
                # Close the connection due to a fatal error.
                self._write(c.args[0].encode('utf-8'))
                self._server_message('!! Closing (%s)' % c.args[0])
                break
            except:
                traceback.print_exc()
                # No break for debugging purpose.

        # You served well, dead Handler. *BANG*
        self._die()


class FakeMPDServer(serv.ThreadingMixIn, serv.TCPServer):
    'Default Implementation of the socketserver'
    allow_reuse_address = True


class ShadowServer:
    'Only visible in the API'
    def __init__(self, host='localhost', port=6666):
        self._host = host
        self._port = port
        self._th = None

    def _loop(self, ready_queue):
        try:
            serv.TCPServer.allow_reuse_address = True
            serv.ThreadingTCPServer.allow_reuse_address = True
            server = serv.ThreadingTCPServer((self._host, self._port), FakeMPDHandler)
            server.gstate = GlobalState()
            print('-- Server running on localhost:6666')
            ready_queue.put(True)
            server.serve_forever()
        except KeyboardInterrupt:
            try:
                server.shutdown()
                print('-- Server shutdown')
            except KeyboardInterrupt:
                print('double tap')

    def _sigint_handler(self, signal, frame):
        print('-- Sending Interrupt to FakeMPD...')
        self.stop()

    def start(self):
        # Make it run in the background.
        import multiprocessing

        # Only used to indicate that the server is actually running
        ready_queue = multiprocessing.Queue()
        self._th = multiprocessing.Process(target=self._loop,
                                           args=(ready_queue, ),
                                           name='FakeMPDWorker')
        self._th.start()

        print('before ')
        # Wait till server is up. (or at least very close to)
        ready_queue.get()
        print('after ')

        # Register signal handler *after* fork()
        signal.signal(signal.SIGINT, self._sigint_handler)

    def stop(self):
        if self._th is not None:
            os.kill(self._th.pid, signal.SIGINT)
            # Seems to send a KeyboardInterrupt to _loop. Good to know.
            self._th.join()
        else:
            raise ValueError('Server has not been started yet!')


if __name__ == '__main__':
    try:
        shad = ShadowServer()
        shad.start()
    except OSError as e:
        print('Cannot create Server (%s). Maybe wait 30 seconds?' % e)
    except KeyboardInterrupt:
        shad.stop()
        print(' Coitus Interruptus.')
