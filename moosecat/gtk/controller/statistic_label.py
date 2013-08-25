from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk, GLib
from moosecat.boot import g


from datetime import datetime, timedelta


TEMPLATE = """\
<small>{elapsed_time}/{total_time} | {num_songs} Songs (~{db_playtime}) | \
{sample_rate} KHz | {bit} Bit | {kbit} KBit/s | {channels}</small>\
"""


def _format_time(time_sec):
    m, s = divmod(time_sec, 60)
    h, m = divmod(m, 60)

    if h > 0:
        return '{:d}:{:02d}:{:02d}'.format(h, m, s)
    else:
        return '{:02d}:{:02d}'.format(m, s)


def _append_unit(markup, num, unit):
    if num > 0:
        if num == 1:
            markup += '1' + unit[:-1]
        else:
            markup += ' '.join((str(num), unit))
        markup += ' '
    return markup


def _format_db_playtime(playtime_sec):
    delta = timedelta(seconds=playtime_sec)
    dtime = datetime(1, 1, 1) + delta

    # - 1 because 0 days are possible (couting stats at 1.1.1970)
    days, hours, minutes = dtime.day - 1, dtime.hour, dtime.minute
    weeks = days / 7

    markup = ''
    markup = _append_unit(markup, weeks, 'Weeks')
    markup = _append_unit(markup, days, 'Days')
    markup = _append_unit(markup, hours, 'Hours')
    markup = _append_unit(markup, minutes, 'Mins')

    return markup.strip()


def _channels_to_string(channels):
    # To my knowledge currently will only give you these two:
    return {
        0: '<small>(( no stream ))</small>',
        1: 'Mono',
        2: 'Stereo'
    }.get(channels, 'Stereo')


class StatisticLabel(Hideable):
    def __init__(self, builder):
        self._stats_label = builder.get_object('statistic_label')
        self._is_playing = True
        GLib.timeout_add(2000, self._on_timeout_event)
        g.client.signal_add(
                'client-event',
                self._on_client_event,
                mask=(Idle.PLAYER | Idle.DATABASE | Idle.UPDATE | Idle.SEEK)
        )

    def _format_text(self, song, stats, status):
        if status is not None:
            status_dict = {
                'sample_rate': status.audio_sample_rate,
                'bit': status.audio_bits,
                'kbit': status.kbit_rate,
                'channels': status.audio_channels
            }
        else:
            status_dict = {
                'sample_rate': 0,
                'bit': 0,
                'kbit': 0,
                'channels': 0
            }

        return TEMPLATE.format(
            elapsed_time=_format_time(int(g.heartbeat.elapsed)),
            total_time=_format_time(int(g.heartbeat.duration)),
            num_songs=stats.number_of_songs,
            db_playtime=_format_db_playtime(stats.db_playtime.tm_sec),
            **status_dict
        )

    def _on_timeout_event(self):
        try:
            self._on_client_event(g.client, Idle.SEEK)
        finally:
            return True

    def _on_client_event(self, client, event):
        with client.lock_all() as all:
            status, song, stats = all
            text = self._format_text(song, stats, status)
            self._stats_label.set_markup(text)
            self._is_playing = (status.state == Status.Playing)
