from gi.repository import Gtk
from moosecat.gtk.widgets.cairo_slider import CairoSlider


class BarSlider(CairoSlider):
    def __init__(self, line_width=2, line_gap=1):
        CairoSlider.__init__(self)
        self._line_width = line_width
        self._line_gap = line_gap

    def on_draw(self, area, ctx):
        alloc = self.get_allocation()
        width, height = alloc.width, alloc.height

        # Theming support
        a_color = self.theme_active_color
        i_color = self.theme_inactive_color

        # True when bars after self._position are drawn
        color_turned = False

        ratio = height / float(width)
        extra_gap = self._line_width + self._line_gap

        # Configure the context to use filling lines and a nice color
        ctx.set_line_width(self._line_width)
        ctx.set_source_rgb(a_color.red, a_color.green, a_color.blue)

        for i in range(0, width, extra_gap):
            # Height of this line
            lim = height - (i * ratio) - 2

            # Draw a straight line up
            ctx.move_to(i, height)
            ctx.line_to(i, lim)

            # No sense in drawing any further
            if lim < 1:
                break
            else:
                if color_turned is False and i >= self._position:
                    # Draw everything till now
                    ctx.stroke()

                    # Change the color for the next bars
                    ctx.set_source_rgb(i_color.red, i_color.green, i_color.blue)
                    color_turned = True

        # Draw the rest
        ctx.stroke()
        return True


if __name__ == '__main__':
    win = Gtk.Window()
    win.connect("delete-event", Gtk.main_quit)
    win.add(BarSlider())
    win.show_all()

    Gtk.main()
