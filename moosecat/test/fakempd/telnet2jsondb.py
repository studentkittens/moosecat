#!/usr/bin/env python
# encoding: utf-8

'''
simple script to convert the output deliverd from MPD to a JSON Representation,
that is readable by fake_mpd.py. You usually do not need this, but urlaub.db
was created this way:

    cat mpd_telnet.out | python telnet2jsondb.py > urlaub.db

'''


import json


if __name__ == '__main__':
    song_list = []
    try:
        song_dict = {}
        while True:
            line = input()
            key, value = [x.strip() for x in line.split(':', maxsplit=1)]
            if key == 'file':
                if len(song_dict) is not 0:
                    song_list.append(song_dict)
                song_dict = {}
            song_dict[key] = value
    except EOFError:
        pass
    finally:
        print(json.dumps(song_list, indent=4, sort_keys=True))
