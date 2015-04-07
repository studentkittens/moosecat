from gi.repository import Moose
from gi.repository import GLib

c = Moose.CmdClient()
c.connect_to('localhost', 6600, 100)
print(c.is_connected())

s = Moose.Store.new(c)
pl = Moose.Playlist()
id_ = s.query('Akrea', False, pl, -1)

# TODO: add signal for op finish
s.wait_for_job(id_)
s.get_result(id_)
print(pl, len(pl))

for song in pl:
    print(song.get_uri())

try:
    loop = GLib.MainLoop()
    GLib.timeout_add(10 * 1000, loop.quit)
    loop.run()
except KeyboardInterrupt:
    print('[Ctrl-C]')
