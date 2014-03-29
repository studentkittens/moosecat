import math
import colorsys

# External:
from gi.repository import Gtk, Gdk

import cairo

# Internal:
from moosecat.boot import g
from moosecat.core import Status, Idle
from moosecat.gtk.utils.drawing import draw_center_text
from moosecat.gtk.widgets import SimplePopupMenu

def draw_rounded_rectangle(ctx, x, y, width, height, aspect, corner_radius):
    radius = corner_radius / aspect
    degs = math.pi / 180.0

    ctx.new_sub_path()
    ctx.arc(x + width - radius, y + radius, radius, -90 * degs, 0)
    ctx.arc(x + width - radius, y + height - radius, radius, 0, 90 * degs)
    ctx.arc(x + radius, y + height - radius, radius, 90 * degs, 180 * degs)
    ctx.arc(x + radius, y + radius, radius, 180 * degs, 270 * degs)
    ctx.close_path()


def add_shadow(ctx, x1, y1, x2, y2, a1=0.42, a2=0.0):
    linear_grad = cairo.LinearGradient(x1, y1, x2, y2)
    linear_grad.add_color_stop_rgba(0, 0.0, 0.0, 0.0, 0.42)
    linear_grad.add_color_stop_rgba(1, 0.5, 0.5, 0.5, 0.00)
    ctx.set_source(linear_grad)
    ctx.paint()


def draw_icon(ctx, x, y, width, height, rgb, symbol, aspect=1.0, corner_radius=20):
    # Draw the bottom Layer:
    ctx.set_source_rgb(*rgb)
    draw_rounded_rectangle(ctx, x, y, width, height,  aspect, corner_radius)
    ctx.fill()

    # Make the bottom layer fade to the left, upper corner.
    add_shadow(ctx, width, height, 0, 0, a1=0.8, a2=0.1)

    # Draw the Top-Layer:
    ctx.set_source_rgb(*rgb)
    draw_rounded_rectangle(
        ctx, x + 2, y + 2, width - width / 12, height - height / 12,
        aspect, corner_radius
    )
    ctx.fill()

    # ADd some glow point on top:
    radial = cairo.RadialGradient(
        width / 2, 0,
        width / 10000,
        width / 2, width / 4,
        width / 3 * 2
    )
    radial.add_color_stop_rgba(0, 1, 1, 1, 0.4)
    radial.add_color_stop_rgba(1, 0, 0, 0, 0.1)
    ctx.set_source(radial)
    ctx.paint()

    # Add some nice shadows left and bottom:
    add_shadow(ctx, width / 2, height, width / 2, height - height / 4)
    add_shadow(ctx, 0, height / 2, width / 7, height / 2)

    # Now draw the center text:
    for idx, alpha in enumerate([0.5, 0.9]):
        ctx.set_source_rgba(idx, idx, idx, alpha)
        offset = height / 7 * (1 - idx)
        draw_center_text(
            ctx, width - offset, height - 0.5 * offset,
            symbol, font_size=height * 0.6
        )


class TrayIcon(Gtk.StatusIcon):
    def __init__(self, title='Moosecat'):
        Gtk.StatusIcon.__init__(self)

        self._hue = 0.21
        self._symbol = '♬'

        self.set_title(title)
        self.set_visible(True)
        self._redraw()

    @property
    def hue(self):
        return self._hue

    @hue.setter
    def hue(self, new_hue):
        self._hue = new_hue
        self._redraw()

    @property
    def symbol(self):
        return self._symbol

    @symbol.setter
    def symbol(self, new_symbol):
        self._symbol = new_symbol
        self._redraw()

    def _redraw(self):
        'Redraw the Icon in the bar'

        # Only render the size needed.
        # get_size() only returns sensitive
        size = self.get_size() if self.is_embedded() else 64

        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
        ctx = cairo.Context(surface)

        rgb = colorsys.hsv_to_rgb(self._hue, 0.85, 0.75)
        draw_icon(
            ctx, 0.5, 0.5, size - 0.5, size - 0.5,
            rgb=rgb, symbol=self._symbol,
            aspect=1.0, corner_radius=size / 7
        )

        pixbuf = Gdk.pixbuf_get_from_surface(
            surface, 0, 0,
            surface.get_width(),
            surface.get_height()
        )

        self.set_from_pixbuf(pixbuf)

STATES = {
    Status.Playing: (0.21, '♬'),
    Status.Paused: (0.08, '♬'),
    Status.Stopped: (0.02, '▣'),
    Status.Unknown: (0.57, '⚡')
}


class TrayIconController:
    def __init__(self):
        self._icon = TrayIcon()
        g.client.signal_add(
            'client-event', self._on_client_event, mask=Idle.PLAYER
        )
        self._icon.connect('scroll-event', self._on_default_scroll_event)
        self._icon.connect('size-changed', self._on_size_changed)
        self._icon.connect('popup-menu', self._on_popup)

        self._create_menu()

    #############
    #  Signals  #
    #############

    def _on_client_event(self, client, event):
        with client.lock_status() as status:
            hue, symbol = STATES[status.state]
            self._icon.symbol = symbol
            self._icon.hue = hue

    def _on_default_scroll_event(self, icon, event):
        if event.direction == Gdk.ScrollDirection.UP:
            sign = +1
        elif event.direction == Gdk.ScrollDirection.DOWN:
            sign = -1
        else:
            sign = 0

        with g.client.lock_status() as status:
            status.volume += sign * 5

    def _on_size_changed(self, tray, new_size):
        self._icon._redraw()

    def _on_popup(self, icon, button, time):
        self._menu.popup(None, None, None, None, button, time)

    def _create_menu(self):
        # Note that the visual appearance is already similar to the final menu.
        # :)
        self._menu = SimplePopupMenu()
        self._menu.simple_add(
            'Show Window',
            callback=lambda *_: g.gtk_app.activate(),
            stock_id='gtk-execute'
        )
        self._menu.simple_add(
            'Preferences',
            callback=lambda *_: g.gtk_sidebar.switch_to('Settings'),
            stock_id='gtk-preferences'
        )
        # ------------------------------
        self._menu.simple_add_separator()
        self._menu.simple_add(
            'Pause',
            callback=lambda *_: g.client.player_pause(),
            stock_id='gtk-media-pause'
        )
        self._menu.simple_add(
            'Previous',
            callback=lambda *_: g.client.player_previous(),
            stock_id='gtk-media-previous'
        )
        self._menu.simple_add(
            'Next',
            callback=lambda *_: g.client.player_next(),
            stock_id='gtk-media-next'
        )
        self._menu.simple_add(
            'Stop',
            callback=lambda *_: g.client.player_stop(),
            stock_id='gtk-media-stop'
        )
        # ------------------------------
        self._menu.simple_add_separator()
        self._menu.simple_add(
            'Quit',
            callback=lambda *_: g.gtk_app.quit(),
            stock_id='gtk-quit'
        )


if __name__ == '__main__':
    from moosecat.gtk.runner import main

    with main() as win:
        TrayIconController()
        win.set_no_show_all(True)
        win.hide()
