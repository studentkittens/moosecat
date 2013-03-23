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
        self._client.signal_add('client-event', self._update, mask=Idle.PLAYER)

    @property
    def elapsed(self):
        '''Returns the approx. number of elapsed seconds of the currently playing song

        :returns: The number of seconds as a float, with milliseconds fraction
        '''
        if self._client.status.state is Status.Playing:
            offset = self._current_time_ms() - self._last_update_tmstp
        else:
            offset = 0

        return (self._client.status.elapsed_ms + offset) / 1000.0

    def _update(self, client, event):
        'client-event callback - updates the update timestamp'
        self._last_update_tmstp = self._current_time_ms()

    def _current_time_ms(self):
        return int(round(time() * 1000))


if __name__ == '__main__':
    from moosecat.boot import boot_base, g
    from gi.repository import GLib

    def timeout_callback(heartbeat):
        print(heartbeat.elapsed)
        return True

    boot_base()
    GLib.timeout_add(500, timeout_callback, Heartbeat(g.client))
    GLib.MainLoop().run()
