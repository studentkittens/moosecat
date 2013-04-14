from gi.repository import Gtk, Gdk, GObject


def build_area(x, y, w, h):
    'Build a Gtk.Rectangle from the passed value'
    area = Gdk.Rectangle()
    area.x, area.y, area.width, area.height = x, y, w, h
    return area


def set_source_from_col(ctx, col):
    'Take a cairo context and set the source to col'
    ctx.set_source_rgb(col.red, col.green, col.blue)


class CellRendererArrow(Gtk.CellRenderer):
    icon = GObject.property(type=str, default=Gtk.STOCK_FIND)
    text = GObject.property(type=str, default='')
    row_color = GObject.property(type=Gdk.Color, default=None)

    def __init__(self):
        Gtk.CellRenderer.__init__(self)
        self._text_renderer = Gtk.CellRendererText()
        self._icon_renderer = Gtk.CellRendererPixbuf()
        self._icon_renderer.set_property('stock-size', Gtk.IconSize.DND)

    def do_render(self, ctx, widget, background_area, cell_area, flags):
        # widget is most likely the containing treeview
        style = widget.get_style_context()

        if self.row_color is not None:
            set_source_from_col(self.row_color)
            ctx.paint()

        # Only draw an arrow when the cell is selected
        if flags & Gtk.CellRendererState.SELECTED:
            # Get the allocation from the cell rectangle
            x, y, width, height = cell_area.x, cell_area.y, cell_area.width, cell_area.height

            # With of the border line
            line_width = 3

            # Paint background in normal color. Gets rid of the usual selection color
            set_source_from_col(ctx, style.get_background_color(Gtk.StateFlags.NORMAL))
            ctx.paint()

            # Draw a simple arrow (a rectangle with a right sharp ending)
            # stop_point is the point where the rectangle ends and the arrow starts
            stop_point = width - 20
            ctx.move_to(x, y + line_width)
            ctx.line_to(x + stop_point, y + line_width)
            ctx.line_to(x + width - line_width, y + height / 2)
            ctx.line_to(x + stop_point, y + height - line_width)
            ctx.line_to(x, y + height - line_width)

            # Draw a small black line around it
            set_source_from_col(ctx, style.get_border_color(Gtk.StateFlags.SELECTED))
            ctx.set_line_width(line_width)

            # But dont delte the context
            ctx.stroke_preserve()

            # Fill the arrow with the selection color used in the theme
            set_source_from_col(ctx, style.get_background_color(Gtk.StateFlags.SELECTED))
            ctx.fill()

        # Width of the icon area
        icon_width = 40

        # Make it easier to write
        x, y, w, h = cell_area.x, cell_area.y, cell_area.width, cell_area.height

        # Determine the area used by the icon and by the text cellrenderer
        icon_area = build_area(x, y, icon_width, h)
        text_area = build_area(x + icon_width, y, w - icon_width, h)

        self._text_renderer.set_property('text', self.text)
        self._icon_renderer.set_property('stock-id', self.icon)

        # Let CellRendererText and CellRendererPixbuf do the actual hard work.
        self._icon_renderer.render(ctx, widget, background_area, icon_area, flags)
        self._text_renderer.render(ctx, widget, background_area, text_area, 0)

    def do_get_size(self, widget, cell_area):
        # FIXME: get_size() doc's say it's deprectated,
        # but do_get_preferred_size does not seem to work yet.
        icon_size = self._icon_renderer.get_size(widget, cell_area)
        text_size = self._icon_renderer.get_size(widget, cell_area)

        width = icon_size[2] + text_size[2] + 2
        height = 40
        return (0, 0, width, height)


if __name__ == '__main__':
    import sys

    class MyApplication(Gtk.Application):
        def do_activate(self):
            # create a Gtk Window belonging to the application itself
            window = Gtk.Window(application=self)
            # set the title
            window.set_title("Cairo Arrow")

            store = Gtk.ListStore(str, str)
            tview = Gtk.TreeView()
            tview.set_model(store)
            tview.append_column(Gtk.TreeViewColumn('Text', CellRendererArrow(), text=0, icon=1))
            tview.set_headers_visible(False)
            tview.set_can_focus(False)

            for name_icon in [('Hackepeter', Gtk.STOCK_FIND),
                              ('Hohelied', Gtk.STOCK_DIALOG_INFO),
                              ('Friede sei mit dir', Gtk.STOCK_DIALOG_WARNING),
                              ('Es wird schlimmer', Gtk.STOCK_DIALOG_ERROR)]:
                store.append(name_icon)

            scw = Gtk.ScrolledWindow()
            scw.add(tview)

            # Only add the cell.
            window.add(scw)

            # show the window
            window.show_all()

    # create and run the application, exit with the value returned by
    # running the program
    app = MyApplication()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
