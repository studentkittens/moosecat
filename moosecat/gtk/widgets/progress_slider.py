#!/usr/bin/env python
# encoding: utf-8

from moosecat.gtk.widgets.cairo_slider import CairoSlider
from cairo import LINE_CAP_SQUARE
from gi.repository import Gtk
from math import pi


class ProgressSlider(CairoSlider):
    def __init__(self, line_width=2):
        # Needs to be first.
        CairoSlider.__init__(self)

        # If False the colored part is not drawn
        self._draw_full_line = True

        # Some const attributes
        self._line_width = line_width
        self._thickness = (self._line_width + 2) / 2

    @property
    def draw_full_line(self, value):
        'If True, color is drawn, if False only the bar is drawn'
        self._draw_full_line = value

    @property
    def draw_full_line(self):
        'Check if we shall draw a full line'
        return self._draw_full_line

    def on_draw(self, area, ctx):
        'Draw a nice rounded bar with cairo, colored to show the progress'

        # Get the space the widget allocates
        alloc = self.get_allocation()
        width, height = alloc.width, alloc.height

        # inactive and active theming colors
        i_color = self.theme_inactive_color
        a_color = self.theme_active_color

        # Precalculate often used values
        radius = height / 2 - self._thickness
        mid = self._thickness + radius
        midx2 = mid * 2
        length = width - midx2
        pid2 = pi / 2

        # Configure the context
        ctx.set_line_cap(LINE_CAP_SQUARE)
        ctx.set_source_rgb(a_color.red, a_color.green, a_color.blue)
        ctx.set_line_width(self._line_width)

        # Left Rounded Corner
        ctx.arc_negative(mid, mid, radius, -pid2, pid2)

        # Upper line
        ctx.move_to(mid, self._thickness)
        ctx.line_to(mid + length, self._thickness)

        # Right Rounded Corner
        ctx.arc(mid + length, mid, radius, -pid2, pid2)

        # Down line
        ctx.move_to(mid, midx2 - self._thickness)
        ctx.line_to(mid + length, midx2 - self._thickness)

        # If we shall draw the full thing: paint a colored rectangle and clip
        # it in. Otherwise, we just use the clip mask and draw it.
        if self._draw_full_line is True:
            ctx.rectangle(mid, self._thickness, length, midx2 - 2 * self._thickness)
            ctx.clip()
            ctx.paint()
        else:
            ctx.stroke()

        if self._draw_full_line is True:
            for col, x, y, w, h in [(a_color, 0, 0, self._position, height),
                    (i_color, self._position, 0, width - self._position, height)]:
                ctx.set_source_rgb(col.red, col.green, col.blue)
                ctx.rectangle(x, y, w, h)
                ctx.fill()

            ctx.stroke()

        # Call no other draw functions
        return True


if __name__ == '__main__':
    win = Gtk.Window()
    win.connect("delete-event", Gtk.main_quit)
    win.add(ProgressSlider())
    win.show_all()

    Gtk.main()
