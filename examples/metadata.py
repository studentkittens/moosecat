#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
import os

# Internal:
from gi.repository import Moose

# External:
from gi.repository import GLib


def _get_full_current_song_uri(cfg, client):
    base_path = cfg['database.root_path']
    if base_path is None:
        return

    full_path = None
    with client.reffed_current_song() as song:
        if song is not None:
            full_path = os.path.join(base_path, song.get_uri())

    if full_path is not None and os.access(full_path, os.R_OK):
        return full_path


def configure_query_by_current_song(client, get_type, amount=1):
    '''
    Configure a Query based on the current song.
    If no current song currently None is returned.

    Otherwise the same as :py:func:`configure_query`

    :returns: a :py:class:`plyr.Query`
    '''
    with client.reffed_current_song() as song:
        if song is not None:
            return configure_query(
                get_type,
                song.get_artist(),
                song.get_album(),
                song.get_title(),
                amount=amount,
            )


def configure_query(
    cfg, client, get_type,
    artist=None, album=None, title=None,
    amount=1, debug=False
):
    ''' Create a :class:`plyr.Query` based on the parameters.
    Not all parameters are required, for cover only artist/album is needed.

    Other values will be read from the config or determined from runtime info
    or the values are left to libglyr to be determined.

    :param get_type: A get_type dictated by plyr ('cover', 'lyrics')
    :param artist: Artist Information
    :param album: Album Information
    :param title: Title information
    :param amount: Number of items to retrieve
    :param debug: Print glyr output to stdout?
    :returns: a ready configured :class:`Moose.MetadataQuery`
    '''
    query = Moose.MetadataQuery(
        type=get_type,
        artist=str(artist), album=str(album), title=str(title),
        number=amount,
        music_path=_get_full_current_song_uri(cfg)
    )

    if debug:
        query.enable_debug()

    return query


if __name__ == '__main__':
    from config import Config
    import sys

    loop = GLib.MainLoop()

    def on_item_received(metadata, query):
        for cache in query.get_results():
            data = cache.props.data
            print('RECV:', data.get_data())
        GLib.idle_add(loop.quit)

    cfg = Config()
    ret = Moose.Metadata(database_location='/tmp')
    ret.connect('query-done', on_item_received)

    query = Moose.MetadataQuery(
        type="cover", artist=sys.argv[1], album=sys.argv[2], download=False
    )
    ret.commit(query)

    GLib.timeout_add(10000, loop.quit)

    try:
        loop.run()
    except KeyboardInterrupt:
        pass
