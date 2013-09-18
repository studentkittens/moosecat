from time import time
from moosecat.core import Idle, Status


class Heartbeat:
    '''Count the elapsed song time without querying MPD.

    It simply looks for signals and calculates the current song position on accessing ``elapsed``.

    .. note::

        For pedantics: Heartbeat assumes it was instanced near to a status update.
        If the last update was long ago it will return slightly wrong results till
        next update. You will never notice it.

    :client: A Client object.
    '''
    def __init__(self, client):
        self._client = client
        self._last_update_tmstp = self._current_time_ms()
        self._client.signal_add(
                'client-event',
                self._on_client_event,
                mask=(Idle.PLAYER | Idle.SEEK)
        )

    @property
    def elapsed(self):
        '''Returns the approx. number of elapsed seconds of the currently playing song

        :returns: The number of seconds as a float, with milliseconds fraction
        '''
        if not self._client.is_connected:
            return 0.0

        elapsed = 0
        with self._client.lock_status() as status:
            if status is not None:
                if status.state is Status.Playing:
                    offset = self._current_time_ms() - self._last_update_tmstp
                else:
                    offset = 0

            elapsed = (status.elapsed_ms + offset) / 1000.0
        return elapsed

    @property
    def duration(self):
        '''Returns the approx. duration of the currently playing song

        :returns: The number of seconds as a float, with milliseconds fraction
        '''
        duration = 0
        with self._client.lock_currentsong() as song:
            if song:
                duration = song.duration
        return duration

    @property
    def percent(self):
        '''
        Convinience method.

        Returns (self.elapsed / self.duration) * 100
        '''
        if self.duration is not 0:
            return self.elapsed / self.duration
        else:
            return 0

    def _on_client_event(self, client, event):
        'client-event callback - updates the update timestamp'
        self._last_update_tmstp = self._current_time_ms()

    def _current_time_ms(self):
        return int(round(time() * 1000))


if __name__ == '__main__':
    from moosecat.boot import boot_base, g
    from gi.repository import GLib

    def timeout_callback():
        #print(g.client.is_connected)
        print(g.heartbeat.elapsed)
        g.client.connect()
        return True

    boot_base(protocol_machine='idle')

    # additional challenge, make first request disconnected
    g.client.disconnect()
    g.client.connect()

    #GLib.timeout_add(500, timeout_callback)

    try:
        GLib.MainLoop().run()
    except KeyboardInterrupt:
        pass
