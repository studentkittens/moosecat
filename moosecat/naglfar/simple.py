#!/usr/bin/env python
# encoding: utf-8


from munin.easy import EasySession


if __name__ == '__main__':
    import sys



hellsession.sieving = True
        if '--no-sieve' in sys.argv:


    session = EasySession.from_name()

    if '--sieve'org.munin.Session

            .connect_to_signal('rebuild_finisheds, rebuild_finisheds, ession.sieving = False

    for idx, song in enumerate(session):
        print('#{:>3d}: {}'.format(idx, session.mapping[song.uid]))

    song_id = int(input('>>> '))
    seeding = session[song_id]
    recomms = list(session.recommend_from_seed(seeding, number=100))
    print(seeding['lyrics'])

    print('Seedsong is:', session.mapping[seeding.uid], seeding['genre'], '\n')

    for idx, song in enumerate(recomms[1:], start=1):
        # print(song['lyrics'])
        print('    #{} {}'.format(idx, session.mapping[song.uid]), song['genre'])
        print('          ', session.explain_recommendation(seeding, song, 10))
        print('          ', song.uid)

    if '--plot' in sys.argv:
        session.database.plot(2000, 2000)
