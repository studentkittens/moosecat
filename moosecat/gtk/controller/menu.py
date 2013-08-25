from moosecat.gtk.interfaces import Hideable
from moosecat.core import Idle, Status
from gi.repository import Gtk, GLib
from moosecat.boot import g


class AboutDialog(Gtk.AboutDialog):
    def __init__(self):
        Gtk.AboutDialog.__init__(self)
        self.set_program_name('Moosecat')
        self.set_version('0.0.1')
        self.set_copyright('GEMA!')
        self.set_license_type(Gtk.License.GPL_3_0)
        self.set_website('https://www.github.com/studentkittens/moosecat')


class Menu(Hideable):
    def __init__(self, builder):
        go = builder.get_object

        def connect_items(*names):
            for name in names:
                builder.get_object('menu_item_' + name).connect(
                        'activate',
                        getattr(self, '_on_activate_' + name)
                )

        # File Menu
        connect_items('connect', 'disconnect', 'quit')

        # Playback Menu
        connect_items(
                'stop', 'play', 'prev', 'next',
                'random', 'single', 'repeat', 'consume'
        )

        # Help Menu
        connect_items('about')


        self._menu_output = builder.get_object('menu_output')
        g.client.signal_add('client-event', self._on_client_event)

    def _on_client_event(self, client, event):
        if event & (Idle.OUTPUT | Idle.OPTIONS):
            for child in self._menu_output.get_children():
                self._menu_output.remove(child)

            with client.lock_outputs() as outputs:
                for output in outputs:
                    print('----------', output)
                    item = Gtk.CheckMenuItem.new_with_label(output.name)
                    item.set_active(output.enabled)
                    self._menu_output.append(item)
            self._menu_output.show_all()

    ###############
    #  Help Menu  #
    ###############

    def _on_activate_about(self, _):
        about_dialog = AboutDialog()
        about_dialog.run()

    ###############
    #  File Menu  #
    ###############

    def _on_activate_connect(self, _):
        g.client.connect()

    def _on_activate_disconnect(self, _):
        g.client.disconnect()

    def _on_activate_quit(self, _):
        g.gtk_app.quit()

    ###################
    #  Playback Menu  #
    ###################

    def _on_activate_play(self, _):
        g.client.player_play()

    def _on_activate_stop(self, _):
        g.client.player_stop()

    def _on_activate_prev(self, _):
        g.client.player_previous()

    def _on_activate_next(self, _):
        g.client.player_next()

    def _on_activate_random(self, item):
        with g.client.lock_status() as status:
            status.random = item.get_active()

    def _on_activate_consume(self, item):
        with g.client.lock_status() as status:
            status.consume = item.get_active()

    def _on_activate_single(self, item):
        with g.client.lock_status() as status:
            status.single = item.get_active()

    def _on_activate_repeat(self, item):
        with g.client.lock_status() as status:
            status.repeat = item.get_active()

    ##################
    #  Outputs Menu  #
    #################
