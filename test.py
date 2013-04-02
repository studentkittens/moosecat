from gi.repository import Gtk, GObject


COUNTER = [0, 0, 0]


class TreeModel(GObject.GObject, Gtk.TreeModel):
    def __init__(self, data):
        self.data = data
        self._num_rows = len(self.data)
        self._num_rows_mone = self._num_rows - 1
        if self.data:
            self._n_columns = 3
        else:
            self._n_columns = 0

        self._itermap = {}
        GObject.GObject.__init__(self)

    def do_iter_next(self, iter_):
        idx = self._itermap.get(iter_)
        if idx is None and self._num_rows is not 0:
            COUNTER[0] += 1
            self._itermap[iter_] = 0
            return (True, iter_)
        elif idx < self._num_rows_mone:
            self._itermap[iter_] += 1
            COUNTER[1] += 1
            return (True, iter_)
        else:
            COUNTER[2] += 1
            return (False, None)


import time
from contextlib import contextmanager


@contextmanager
def timing(msg):
    now = time.time()
    yield
    print(msg, ":", time.time() - now)


if __name__ == '__main__':
    NUM = int(3507831 / 1.0)

    with timing('init'):
        s = TreeModel([('Albert', 'Berta', 'Christoph') for x in range(NUM)])

    with timing('iter'):
        iter_ = Gtk.TreeIter()
        iter_.user_data = 0
        for n in range(NUM):
            s.do_iter_next(iter_)

    print('branch:', COUNTER)
