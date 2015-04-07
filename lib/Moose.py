from ..module import get_introspection_module

# We cannot use the normal `from gi.repository import Moose`.
# Since we're inside the import process already.
Moose = get_introspection_module('Moose')

########################
#  PLAYLIST OVERRIDES  #
########################

def _iter_playlist(playlist):
    for idx in range(playlist.length()):
        yield playlist.at(idx)

Moose.Playlist.__len__ = Moose.Playlist.length
Moose.Playlist.__getitem__ = Moose.Playlist.at
Moose.Playlist.__iter__ = _iter_playlist


########################
#  ZEROCONF OVERRIDES  #
########################

def _iter_browser(browser):
    for server in browser.get_server_list():
        yield server


Moose.ZeroconfBrowser.__iter__ = _iter_browser

VALID_ATTRS = ['name', 'protocol', 'domain', 'addr', 'host', 'port']


def _get_server_attr(server, attr):
    if attr not in VALID_ATTRS:
        raise TypeError('No such attribute in ZeroconfServer: ' + attr)

    # Can't use getattr here and no __dict__ available.
    return eval('server.get_' + attr + '()', {}, {'server': server})


def _iter_server(server):
    for attr in VALID_ATTRS:
        yield (attr, server[attr])


Moose.ZeroconfServer.__getitem__ = _get_server_attr
Moose.ZeroconfServer.__iter__ = _iter_server
Moose.ZeroconfServer.valid_attrs = frozenset(VALID_ATTRS)
