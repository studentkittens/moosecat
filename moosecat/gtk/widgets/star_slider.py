#!/usr/bin/env python
# encoding: utf-8

from moosecat.gtk.widgets.cairo_slider import CairoSlider
from cairo import LINE_CAP_ROUND, LINE_JOIN_ROUND, OPERATOR_IN
import cairo
from gi.repository import Gtk
from math import pi


STAR_POINTS = [
    (0.000, 0.425),
    (0.375, 0.375),
    (0.500, 0.050),
    (0.625, 0.375),
    (1.000, 0.425),
    (0.750, 0.625),
    (0.800, 0.950),
    (0.500, 0.750),
    (0.200, 0.950),
    (0.250, 0.625),
    (0.000, 0.425)
]


class StarSlider(CairoSlider):
    def __init__(self, stars=5, offset=0.1):
        # Needs to be first.
        CairoSlider.__init__(self)
        self._stars, self._offset = stars, offset

    def width_multiplier(self):
        return self._stars + (self._stars - 1) * self._offset

    @property
    def stars(self):
        return self._stars * self.percent

    def on_draw(self, area, ctx):
        # Get the space the widget allocates
        alloc = self.get_allocation()
        width, height = alloc.width, alloc.height

        ctx.set_line_width(2)
        ctx.set_line_cap(LINE_CAP_ROUND)
        ctx.set_line_join(LINE_JOIN_ROUND)

        for i in range(self._stars):
            for idx, (x, y) in enumerate(STAR_POINTS):
                x = x * height + (i * height) * (self._offset + 1.0)
                y = y * height

                if idx is 0:
                    ctx.move_to(x, y)
                else:
                    ctx.line_to(x, y)

        # inactive and active theming colors
        i_color = self.theme_inactive_color
        a_color = self.theme_active_color

        ctx.stroke_preserve()
        ctx.save()
        ctx.clip()
        ctx.rectangle(0, 0, self._position, height)
        ctx.clip()

        ctx.set_source_rgb(a_color.red, a_color.green, a_color.blue)
        ctx.paint()

        ctx.restore()
        ctx.reset_clip()
        ctx.set_source_rgb(i_color.red, i_color.green, i_color.blue)
        ctx.stroke()

        print(self.stars)


if __name__ == '__main__':
    win = Gtk.Window()
    win.connect("delete-event", Gtk.main_quit)
    slider = StarSlider()

    slider.set_size_request(slider.width_multiplier() * 100, 100)
    win.add(slider)
    win.show_all()

    Gtk.main()
