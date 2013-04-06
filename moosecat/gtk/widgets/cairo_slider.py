from cairo import LINE_CAP_SQUARE
from gi.repository import Gtk, Gdk


class CairoSlider(Gtk.DrawingArea):
    def __init__(self):
        Gtk.DrawingArea.__init__(self)

        # Theming Information (so cairo widgets look natural)
        self._style_context = self.get_style_context()

        # Progress from 0 - allocation.width
        self._position = 0

        # True when user drags on the widget with the mouse
        self._drag_mode = False

        # Enable the receival of the appropiate signals:
        self.add_events(self.get_events()         |
                Gdk.EventMask.BUTTON_PRESS_MASK   |
                Gdk.EventMask.BUTTON_RELEASE_MASK |
                Gdk.EventMask.POINTER_MOTION_MASK |
                Gdk.EventMask.SCROLL_MASK
        )

        # Signals used to know when redrawing is desired
        self.connect('scroll-event', self.on_scroll_event)
        self.connect('button-press-event', self.on_button_press_event)
        self.connect('button-release-event', self.on_button_release_event)
        self.connect('motion-notify-event', self.on_motion_notify)
        self.connect('draw', self.on_draw)

    @property
    def theme_active_color(self):
        return self._style_context.get_background_color(Gtk.StateFlags.SELECTED)

    @property
    def theme_inactive_color(self):
        return self._style_context.get_color(Gtk.StateFlags.NORMAL)

    @property
    def percent(self):
        return min(max(float(self._position) / self.get_allocation().width, 0.0), 1.0)

    @percent.setter
    def percent(self, value):
        clamped = min(max(value, 0.0), 1.0)
        self._position = clamped * self.get_allocation().width

    ####################
    #  Widget Signals  #
    ####################

    def on_button_press_event(self, widget, event):
        self._position = event.x
        self.queue_draw()
        self._drag_mode = True
        return True

    def on_button_release_event(self, widget, event):
        self._drag_mode = False
        return True

    def on_scroll_event(self, widget, event):
        if event.direction == Gdk.ScrollDirection.UP:
            offset = +10
        else:
            offset = -10

        self._position = self._position + offset
        self.queue_draw()
        return True

    def on_motion_notify(self, widget, event):
        if self._drag_mode:
            self._position = event.x
            self.queue_draw()

    def on_draw(self, widget, ctx):
        # To filled by subclass
        pass
