#!/usr/bin/env python
# encoding: utf-8

# Stdlib:
from time import time

# Internal:
from gi.repository import Moose

# External:
from gi.repository import GLib


class Heartbeat:
    '''Count the elapsed song time without querying MPD.

    It simply looks for signals and calculates the current song position on
    accessing ``elapsed``.

    .. note::

        For pedantics: Heartbeat assumes it was instanced near to a status
        update.  If the last update was long ago it will return slightly wrong
        results till next update. You will likely never notice in reality.

    :client: A Client object.
    '''
    def __init__(self, client, use_listened_counter=True):
        self._client = client
        self._last_song_queue_pos = -1
        self._last_update_tmstp = self._current_time_ms()

        self._interval = 200
        self._curr_listened = self._last_listened = self._last_duration = 0.0

        self._client.connect(
            'client-event',
            self._on_client_event,
        )

        if use_listened_counter:
            self._last_tick = self._current_time_ms()
            GLib.timeout_add(self._interval, self._on_poll_elapsed)

    def _on_poll_elapsed(self):
        'Update every timeslice the player is running a counter'
        if not self._client.is_connected:
            return 0.0

        now = self._current_time_ms()
        with self._client.reffed_status() as status:
            if status is not None and status.get_state() is Moose.State.PLAY:
                self._curr_listened += now - self._last_tick

        with self._client.reffed_current_song() as song:
            if song is not None:
                self._last_duration = song.get_duration()

        self._last_tick = now
        return True

    @property
    def currently_listened_percent(self):
        '''The percent of the song that was actually listened to.

        This will is resistant against seeking in the song, therefore
        more than 100 percent is possible (imagine listening the whole song
        and skipping back to the beginning).
        '''
        secs = self._curr_listened / (1000 / self._interval * self._interval)
        duration = self._last_duration
        if duration != 0.0:
            return secs / duration
        return 0

    @property
    def last_listened_percent(self):
        '''Same as ``currently_listened_percent``, but for the last listened song.

        This is provided for your convinience
        '''
        return self._last_listened

    @property
    def elapsed(self):
        '''Returns the approx. number of elapsed seconds of the currently playing song

        :returns: The number of seconds as a float, with milliseconds fraction
        '''
        if not self._client.is_connected:
            return 0.0

        elapsed = 0
        with self._client.reffed_status() as status:
            if status is not None:
                if status.get_state() is Moose.State.PLAY:
                    offset = self._current_time_ms() - self._last_update_tmstp
                else:
                    offset = 0

            elapsed = (status.get_elapsed_ms() + offset) / 1000.0
        return elapsed

    @property
    def duration(self):
        '''Returns the approx. duration of the currently playing song

        :returns: The number of seconds as a float, with milliseconds fraction
        '''
        if not self._client.is_connected:
            return 0.0

        duration = 0
        with self._client.reffed_current_song() as song:
            if song is not None:
                duration = song.get_duration()
        return duration

    @property
    def percent(self):
        '''Convinience property.

        Returns self.elapsed / self.duration
        '''
        duration = self.duration
        if duration != 0.0:
            return self.elapsed / duration
        return 0

    def _on_client_event(self, client, event):
        'client-event callback - updates the update timestamp'
        if not event & (Moose.Idle.IDLE_PLAYER | Moose.Idle.IDLE_SEEK):
            return

        song_queue_pos = -1
        with client.reffed_current_song() as song:
            if song is not None:
                song_queue_pos = song.get_pos()

        if self._last_song_queue_pos != song_queue_pos:
            self._last_listened = self.currently_listened_percent
            self._curr_listened = self.elapsed * 1000.0
            self._last_song_queue_pos = song_queue_pos

        self._last_update_tmstp = self._current_time_ms()

    def _current_time_ms(self):
        return time() * 1000


if __name__ == '__main__':
    client = Moose.CmdClient()
    client.connect_to("localhost", 6600, 200)
    hb = Heartbeat(client)

    def timeout_callback():
        print(
            'elapsed={:3.3f} percent={:3.3f} curr={:3.3f} last={:3.3f}'.format(
                hb.elapsed,
                hb.percent,
                hb.currently_listened_percent,
                hb.last_listened_percent
            )
        )

        return True

    GLib.timeout_add(500, timeout_callback)

    try:
        GLib.MainLoop().run()
    except KeyboardInterrupt:
        pass
