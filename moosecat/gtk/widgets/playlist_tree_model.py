from gi.repository import Gtk, Pango, GObject, GLib
from functools import lru_cache

# Note:
# Code below is somewhat optimized and is a little harder to read then I'd like.
# Things to note:
#  * Access of iter_.user_data is slow, so always save a reference
#  * Access of user_data is slow (involves g_property_get())
#  * Only suitable for Lists, not for Trees.


class PlaylistTreeModel(GObject.GObject, Gtk.TreeModel):
    def __init__(self, data, n_columns=None):
        super(PlaylistTreeModel, self).__init__()

        self.data = data
        self._num_rows = len(self.data)
        self._num_rows_minus_one = self._num_rows - 1

        if n_columns is None:
            # Get the length of the first tuple
            self._n_columns = len(self.data[0]) if self.data else 0
        else:
            self._n_columns = n_columns

    @lru_cache(maxsize=256)
    def __getitem__(self, path):
        return self.data[path.get_indices()[0]]

    def do_get_iter(self, path):
        """Returns a new TreeIter that points at path.

        The implementation returns a 2-tuple (bool, TreeIter|None).
        """
        idx = path.get_indices()[0]

        if idx < self._num_rows:
            iter_ = Gtk.TreeIter()
            iter_.user_data = idx
            return (True, iter_)
        else:
            return (False, None)

    def do_iter_next(self, iter_):
        """Returns an iter pointing to the next row or None.

        The implementation returns a 2-tuple (bool, TreeIter|None).
        """
        ud = iter_.user_data
        if ud is None and self._num_rows is not 0:
            iter_.user_data = 0
            return (True, iter_)
        elif ud < self._num_rows_minus_one:
            iter_.user_data = ud + 1
            return (True, iter_)
        else:
            return (False, None)

    def do_iter_previous(self, iter_):
        ud = iter_.user_data
        if ud > 0:
            iter_.user_data = ud - 1
            return (True, iter_)
        else:
            return (False, None)

    def do_iter_has_child(self, iter_):
        """True if iter has children."""
        return False

    def do_iter_nth_child(self, parent, n):
        """Return iter that is set to the nth child of iter."""
        # We've got a flat list here, so iter_ is always None and the
        # nth child is the row.
        if parent is None and 0 <= n < self._num_rows:
            iter_ = Gtk.TreeIter()
            iter_.user_data = n
            return (True, iter_)
        return (False, None)

    def do_get_path(self, iter_):
        """Returns tree path references by iter."""
        ud = iter_.user_data
        if ud is not None:
            return Gtk.TreePath((ud,))
        else:
            return None

    def do_get_value(self, iter_, column):
        """Returns the value for iter and column."""
        ud = iter_.user_data
        if ud is not None:
            return self.data[ud][column]

    def do_get_n_columns(self):
        """Returns the number of columns."""
        return self._n_columns

    def do_get_column_type(self, column):
        """Returns the type of the column."""
        return str

    def do_get_flags(self):
        """Returns the flags supported by this interface."""
        return Gtk.TreeModelFlags.ITERS_PERSIST | Gtk.TreeModelFlags.LIST_ONLY

    #######################
    #  Utility Functions  #
    #######################

    def data_from_path(self, path):
        return self.data[path.get_indices()[0]]
