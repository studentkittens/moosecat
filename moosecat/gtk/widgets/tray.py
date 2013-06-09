#!/usr/bin/env python
# encoding: utf-8

from gi.repository import Gtk, Gdk, GLib, Pango, PangoCairo
from cairo import Context, ImageSurface, RadialGradient, FORMAT_ARGB32
from math import pi


def draw_center_text(ctx, width, height, text, font_size=15):
    'Draw a text at the center of width and height'
    layout = PangoCairo.create_layout(ctx)
    font = Pango.FontDescription()
    font.set_size(font_size * Pango.SCALE)
    layout.set_font_description(font)
    layout.set_text(text, -1)

    fw, fh = [num / Pango.SCALE / 2 for num in layout.get_size()]
    ctx.move_to(width / 2 - fw, height / 2 - fh)
    PangoCairo.show_layout(ctx, layout)


def draw_stopped(ctx, width, height, mx, my):
    'Draw a rectangle at (mx, my)'
    rx, ry = width / 7, height / 7

    ctx.rectangle(mx - rx, mx - ry, 2 * rx, 2 * ry)
    ctx.fill()


def draw_paused(ctx, width, height, mx, my):
    'Draw two vertical bars (like a H without the middlebar)'
    rx, ry = width / 6, height / 6
    lx = rx * 2 / 2.5

    ctx.rectangle(mx - rx, my - ry, lx, 2 * ry)
    ctx.rectangle(mx + lx / 2, my - ry, lx, 2 * ry)
    ctx.fill()


def draw_playing(ctx, width, height, mx, my):
    'Draw a rectangle (centroid at mx, my)'
    rx, ry = width / 5, height / 5

    # move the triangle to the centroid
    # this improves appearance
    mx += rx / 3

    # Paint the Triangle
    ctx.line_to(mx + rx, my)
    ctx.line_to(mx - rx, my + ry)
    ctx.line_to(mx - rx, my - ry)
    ctx.fill()


def draw_undefined(ctx, width, height, mx, my):
    'Undefined state: Draw a Question Mark'
    # Draw a nice unicode symbol in the middle
    draw_center_text(ctx, width, height, '?', font_size=0.45 * min(width, height))


DRAW_STATE_MAP = {
    'playing': draw_playing,
    'stopped': draw_stopped,
    'paused': draw_paused,
    'undefined': draw_undefined
}


def draw_status_icon(state, col_insens, col_active, col_middle, segments=5, percent=0, width=70, height=70):
    '''
    Actually draw the Gtk.StatusIcon

    :state: One of DRAW_STATE_MAP.keys()
    :col_insens: unreached Wheelcolor
    :col_active: reached Wheelcolor
    :col_middle: Color of state symbol
    :segments: Wheelsegments to darw
    :percent: [0.0-1.0] Define how many segments to be active
    :width: Width of resulting pixbuf in Pixel
    :height: Height of resulting pixbuf in Pixel

    :returns: a Pixbuf that can be set on Gtk.StatusIcon.
    '''
    # Create a new Surface (we get a Gdk.Pixbuf from it later)
    surface = ImageSurface(FORMAT_ARGB32, width, height)

    # For actual drawing we need a Context
    ctx = Context(surface)

    # Middle points
    mx, my = width / 2, height / 2

    # Border Width
    bw = min(width, height) / 8
    ctx.set_line_width(bw)

    # Number of segments and length in radiants
    base = (pi * 2) / segments
    left90 = (pi / 2)
    radius = min(mx, my) - (bw / 2 + 1)

    # Draw a black circle in the middle of the segments (non-touching)
    rg = RadialGradient(mx, my, radius / 2, mx, my, min(mx, my))
    rg.add_color_stop_rgb(0, 0.1, 0.1, 0.1)
    rg.add_color_stop_rgb(1, 0.4, 0.4, 0.4)
    ctx.set_source(rg)
    ctx.arc(mx, my, min(mx, my) - 1, 0, 2 * pi)
    ctx.fill()

    # Draw {segments} curvy segments
    for segment in range(segments):
        # Decide which color the segment should have
        if segment / segments * 100 >= percent:
            ctx.set_source_rgb(*col_insens)
        else:
            ctx.set_source_rgb(*col_active)

        # Draw a segment
        ctx.arc(
            mx, my, radius,
            segment * base - left90,
            (0.75 + segment) * base - left90
        )
        ctx.stroke()

    # Now draw an informative symbol into the middle of the black circle
    ctx.set_source_rgb(*col_middle)
    DRAW_STATE_MAP[state](ctx, width, height, mx, my)

    # Convert the surface to an actual Gdk.Pixbuf we can set to the StatusIcon
    return Gdk.pixbuf_get_from_surface(
            surface, 0, 0,
            surface.get_width(),
            surface.get_height()
    )


class TrayIcon(Gtk.StatusIcon):
    def __init__(self, title='Moosecat'):
        Gtk.StatusIcon.__init__(self)

        self.set_title(title)

        self.set_visible(True)
        self.connect('popup-menu', self.on_showpopup)
        self.connect('scroll-event', self.on_default_scroll_event)

        # Colors - fixed, not from theme
        self._col_active = (0.09, 0.57, 0.82)
        self._col_insens = (0.25, 0.25, 0.25)
        self._col_middle = (0.90, 0.90, 0.90)

        # Settings:
        self._percent = 0
        self._state = 'undefined'
        self._segments = 5
        self._on_popup_func = None

        self._redraw()

        # Popups:
        self._menu_items = []

    def _percent_redraw_needed(self, new_percent):
        'Checks if percent altered enough'
        def segment(percent):
            for segment in range(self._segments + 1):
                if (segment / self._segments * 100) >= percent:
                    return segment + 1
            else:
                return self._segments
        return segment(self._percent) != segment(new_percent)

    def _redraw(self):
        'Redraw the Icon in the bar'
        pixbuf = draw_status_icon(
                self._state,
                col_active=self._col_active,
                col_insens=self._col_insens,
                col_middle=self._col_middle,
                segments=self._segments,
                percent=self._percent
        )
        self.set_from_pixbuf(pixbuf)

    #############
    #  Signals  #
    #############

    def on_showpopup(self, icon, button, time):
        if self._on_popup_func:
            menu = Gtk.Menu()
            self._on_popup_func(self, menu)

            menu.show_all()
            menu.popup(
                    None, None,
                    lambda *_: self.position_menu(menu, self),
                    self,
                    3,
                    time
            )

    def on_default_scroll_event(self, icon, event):
        if event.direction == Gdk.ScrollDirection.UP:
            sign = +1
        elif event.direction == Gdk.ScrollDirection.DOWN:
            sign = -1
        else:
            sign = 0

        if sign is not 0:
            self.percent += sign * (50 / self._segments)

    ################
    #  Properties  #
    ################

    def get_percent(self):
        return self._percent

    def set_percent(self, val):
        redraw_needed = self._percent_redraw_needed(val)
        self._percent = min(max(val, 0), 100)

        if redraw_needed:
            self._redraw()

    percent = property(get_percent, set_percent, doc='Percentage of active Wheel')

    def get_state(self):
        return self._state

    def set_state(self, val):
        keys = DRAW_STATE_MAP.keys()
        if val in keys:
            redraw_needed = self._state != val
            self._state = val

            if redraw_needed:
                self._redraw()
        else:
            raise ValueError('Must be one of ' + str(keys))

    state = property(get_state, set_state, doc='State of Icon (one of DRAW_STATE_MAP)')

    def get_on_popup(self):
        return self._on_popup_func

    def set_on_popup(self, func):
        self._on_popup_func = func

    on_popup = property(get_on_popup, set_on_popup, doc='Callback on menu popup')



if __name__ == '__main__':
    def increment_percent(ti):
        from random import choice
        ti.percent += 10
        if ti.percent >= 100:
            ti.percent = 0
            ti.state = choice(list(DRAW_STATE_MAP.keys()))

        ti.set_tooltip_markup('Song at {0:02.1f}%'.format(ti.percent))
        return True

    def on_popup(ti, menu):
        def cm(icon, text, stock_id=None, signal=None):
            menu_item = Gtk.ImageMenuItem(text)
            if signal is not None:
                menu_item.connect('button-press-event', signal)

            if stock_id is not None:
                image = Gtk.Image()
                image.set_from_stock(stock_id, Gtk.IconSize.MENU)
                menu_item.set_image(image)

            menu.append(menu_item)

        sigquit = lambda *a: Gtk.main_quit()
        cm(ti, 'Stop', stock_id='gtk-media-stop', signal=sigquit)
        cm(ti, 'Pause', stock_id='gtk-media-pause', signal=sigquit)
        cm(ti, 'Previous', stock_id='gtk-media-previous', signal=sigquit)
        cm(ti, 'Next', stock_id='gtk-media-next', signal=sigquit)
        menu.append(Gtk.SeparatorMenuItem())
        cm(ti, 'Quit', stock_id='gtk-quit', signal=sigquit)

    def main():
        ti = TrayIcon()
        ti.on_popup = on_popup
        GLib.timeout_add(200, increment_percent, ti)
        Gtk.main()

    main()
