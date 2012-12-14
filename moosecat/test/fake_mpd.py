import socketserver as serv
import socket
import json

MPD_VERSION = b'0.17.0'


###########################################################################
#                           Response Functions                            #
###########################################################################


def respond_status(state):
    return state.status


def respond_stats(state):
    return state.stats


def respond_currentsong(state):
    return state.currentsong


def respond_clear(state):
    state.queue = Playlist()


def respond_add(state):
    state.queue.append(state.args[0])


def respond_consume(state):
    state.status.consume = state.args[0]


def respond_crossfade(state):
    state.status.crossfade = state.args[0]


def respond_delete(state):
    del state.queue[state.args[0]]
    state.status.playlist += 1


###########################################################################
#                          Protocol Description                           #
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

# Commands that are not implemented and just get ACK'd
NOT_IMPLEMENTED = ['channels', 'clearerror', 'commands', 'config',
                   'decoders', 'notcommands', 'password', 'ping',
                   'readmessages', 'sendmessage', 'subscribe', 'tagtypes',
                   'unsubscripe', 'urlhandlers']

# Commands that need to implemented at Socket level,
# i.e. no request_handler is called.
SOCKET_LEVEL = ['close', 'kill', 'idle', 'noidle']


# TODO
# outputs, enableoutput, disableoutput

PROTOCOL = {
    'status': [0, 0, respond_status],
    'currentsong': [0, 0, respond_currentsong],
    'stats': [0, 0, respond_stats],
    # Queue
    'add': [1, 1, respond_add],
    'clear': [0, 0, respond_clear],
    'consume': [1, 1, respond_consume],
    'crossfade': [1, 1, respond_crossfade],
    'count': lambda state: 0, 'delete': [1, 1, respond_delete]
}


###########################################################################
#                              Utils                                      #
###########################################################################


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


###########################################################################
#                                 Errors                                  #
###########################################################################


class Error:
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
    def __init__(self, state, cause, errid=Error.SERVER):
        self._state = state
        self._cause = cause
        self._errid = errid
        Exception.__init__(str(self))

    def __str__(self):
        return 'ACK [{error_id}@{cmnd_list_num}] {{{current_cmd}}} {cause}'.format(
            error_id=self.errid,
            cmnd_list_num=self._state.command_list_idx,
            current_cmd=self._state.current_command,
            cause=self._cause
        )


###########################################################################
#                              State Classes                              #
###########################################################################


class ProtocolState:
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

        # State Data (Default)
        self.status = Status()
        self.currentsong = Song()
        self.stats = Stats()

        # Playlists / Database
        self.queue = Playlist()
        self.db = Playlist.from_json('./urlaub.db')


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

    def from_dict(song_dict):
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
    def __init__(self, songs=[]):
        self._songs = songs

    def append(self, song):
        self._songs.append(song)

    def __getitem__(self, idx):
        return self._songs[idx]

    def from_json(path):
        instance = Playlist()
        with open(path, 'r') as f:
            for node in json.load(f):
                instance.append(Song.from_dict(node))
        return instance


###########################################################################
#                               Networking                                #
###########################################################################


class FakeMPDHandler(serv.StreamRequestHandler):
    def respond(self, message):
        # Convert the incoming object to a string,
        # and then encode it as UTF-8 bytestring (needed for sockets)
        # Also checks if it alreay is a bytestring.
        if not isinstance(message, bytes):
            b_message = str(message).encode('utf-8')
        else:
            b_message = message


        # Check if an Error happened.
        if isinstance(message, ProtocolError):
            self.wfile.write(b_message)
        else:
            # Okay the message. If no output was written,
            # we just write the OK.
            if len(b_message) is not 0:
                # Write it back to the client
                self.wfile.write(b_message)

                # New line for OK...
                self.wfile.write(b'\n')
                self.wfile.write(b'OK\n')


    def lookup(self, cmnd):
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
        cmnd_name = str(split[0], 'ascii')

        # See if we have a handler for this command
        min_args, max_args, respond_func = PROTOCOL.get(cmnd_name, [0, 0, None])
        if respond_func is not None:

            # Remember the current command for the duration
            # of a request. It's undefined afterwards.
            self.state.current_command = cmnd_name

            # See if the command has additional arguments.
            # If so we try to convert them to integers, or leave them as it is.
            # (Just makes stuff easier)
            if len(split) > 1:
                self.state.args = list(map(keep_or_convert_to_int, split[1:]))
            else:
                self.state.args = []

            # See if the number of arguments fits.
            # If so we *return* an Exception. Might be a weird, but this
            # special Exception can be converted easily to a 'ACK ...' string.
            if not (min_args <= len(split) <= max_args):
                return ProtocolError()

            # Try to run the Respond Func.
            # If it fails it might throw a ProcotolError with the reason.
            # The ProtocolError can be converted to a ready to use bytemessage,
            # describing the error on a Protocol level.
            try:
                # Can return arbitary python objects
                return respond_func(self.state)
            except ProtocolError as err:
                # Gets converted to a str later
                return err
        else:
            # Just assume the Command is not implemented on the serverside, and
            # therefore we silently OK it. A List of commands not being
            # implemented can be found in NOT_IMPLEMENTED.
            return ''

    def __init(self):
        '''
        Initialize the Handler object.
        It is only called once per calling handle()
        '''
        # There is one State per Handler
        self.state = ProtocolState()

    def handle(self):
        '''
        handle() is called once a connection is established on the Server.
        Once handle returns the connection is closed
        '''
        self.__init()

        # Send the greeting message as demanded
        self.wfile.write(b'OK MPD ' + MPD_VERSION + b'\n')

        # While the client does not quit, we try to read input from them.
        while True:
            try:
                # Read a single line
                cmnd = self.rfile.readline().strip()
                print('CMND: ', str(cmnd))

                # Lookup a response function (,,process that shit!'')
                resp = self.lookup(cmnd)

                # Write it back to the client
                self.respond(resp)
            except socket.error as e:
                print('Connection closed (%s)' % e)
                break


class FakeMPDServer(serv.ThreadingMixIn, serv.TCPServer):
    'Default Implementation of the socketserver'
    pass


if __name__ == '__main__':
    try:
        server = serv.ThreadingTCPServer(('localhost', 6666), FakeMPDHandler)
        print('-- Server running on localhost:6666')
        server.serve_forever()
    except OSError as e:
        print('Cannot create Server (%s). Maybe wait 30 seconds?' % e)
    except KeyboardInterrupt:
        server.shutdown()
        print(' Coitus Interruptus.')
