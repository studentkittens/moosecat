#!/usr/bin/env python
# encoding: utf-8

# Internal:
from moosecat.boot import g
from moosecat.gtk.metadata.templates import listbox_create_labelrow, add_label_to_grid

# External
from gi.repository import Gtk, GObject
import plyr


###########################################################################
#                              Util Widgets                               #
###########################################################################

class Section(Gtk.Box):
    def __init__(self, title, content_widget):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.VERTICAL)

        self._content_widget = content_widget

        label = Gtk.Label('<b>{t}:</b>'.format(t=title))
        label.set_use_markup(True)
        label.set_alignment(0, 0.5)

        self._expander = Gtk.Expander()
        self._expander.set_expanded(True)

        button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        button_box.pack_start(self._expander, False, False, 0)
        button_box.pack_start(Gtk.VSeparator(), False, False, 0)
        button_box.pack_start(label, True, True, 5)

        toggle_button = Gtk.Button()
        toggle_button.add(button_box)
        toggle_button.connect('clicked', self._on_toggle_content_visibility)
        toggle_button.set_hexpand(True)

        self._content_widget.set_vexpand(True)
        self._content_widget.set_hexpand(True)

        self.pack_start(toggle_button, False, True, 0)
        self.pack_start(self._content_widget, True, True, 0)

        self.set_halign(Gtk.Align.FILL)
        self.set_valign(Gtk.Align.FILL)

    def _on_toggle_content_visibility(self, button):
        new_state = not self._expander.get_expanded()
        self._expander.set_expanded(new_state)
        self._content_widget.set_visible(new_state)


class LabeledLevelBar(Gtk.Box):
    __gsignals__ = {
        'changed': (
            GObject.SIGNAL_RUN_FIRST,  # Run-Order
            None,                      # Return Type
            (float, )                  # Parameters
        )
    }

    def __init__(self, multiplier=1, label_format='{:2.3f}'):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.HORIZONTAL)

        self._multiplier = multiplier
        self._label_format = label_format

        self._levelbar = Gtk.LevelBar.new_for_interval(0.0, 1.0)
        self._levelbar.set_size_request(95, -1)
        self._label = Gtk.Label()

        self._levelbar.add_offset_value(Gtk.LEVEL_BAR_OFFSET_LOW, 0.33)
        self._levelbar.add_offset_value(Gtk.LEVEL_BAR_OFFSET_HIGH, 0.66)

        eventbox = Gtk.EventBox()
        eventbox.add(self._levelbar)
        eventbox.connect('button-press-event', self._on_button_press_event)

        self.pack_start(self._label, False, False, 3)
        self.pack_start(eventbox, True, True, 0)

    def _on_button_press_event(self, widget, event):
        self._percent = event.x / widget.get_allocation().width
        self.set_value(self._percent * self._multiplier, emit=False)

    def set_value(self, value, emit=True):
        self._percent = value / self._multiplier
        self._levelbar.set_value(self._percent)
        self._label.set_text(self._label_format.format(self.get_value()))
        if emit is True:
            self.emit('changed', self._percent)

    def get_value(self):
        return self._percent * self._multiplier


###########################################################################
#                          The Settings Sections                          #
###########################################################################

class ProviderSection(Section):
    def __init__(self):
        self._model = None
        self._provider = Gtk.TreeView()
        self._chosen_type = None

        Section.__init__(self, 'Provider', self._provider)

        cell = Gtk.CellRendererToggle()
        cell.set_radio(True)
        cell.connect('toggled', self._on_toggle_cell)
        col = Gtk.TreeViewColumn('Active', cell, active=0)
        self._provider.append_column(col)

        for idx, name in enumerate(['Provider', 'Quality', 'Speed'], start=1):
            if idx > 1:
                cell = Gtk.CellRendererProgress()
                col = Gtk.TreeViewColumn(name, cell, text=idx, value=idx)
            else:
                cell = Gtk.CellRendererText()
                col = Gtk.TreeViewColumn(name, cell, text=idx)
                col.set_expand(True)
            col.set_sort_column_id(idx)
            self._provider.append_column(col)

    def update_provider(self, chosen_type):
        self._chosen_type = chosen_type.lower()
        if self._chosen_type is None:
            return

        providers = plyr.PROVIDERS[self._chosen_type]['providers'] + [{
            'name': 'local',
            'speed': 100,
            'quality': 100
        }, {
            'name': 'musictree',
            'speed': 99,
            'quality': 99
        }]
        self._model = Gtk.ListStore(bool, str, int, int)

        # Lookup which one are enabled:
        enabled = set(g.config.get(
            'metadata.providers.' + self._chosen_type
        ))

        for p in sorted(providers, key=lambda e: e['quality'], reverse=True):
            is_enabled = 'all' in enabled or p['name'] in enabled
            self._model.append([
                is_enabled, p['name'], p['quality'], p['speed']
            ])

        self._provider.set_model(self._model)

    def _on_toggle_cell(self, cell, path):
        if path is not None:
            it = self._model.get_iter(path)
            self._model[it][0] = not self._model[it][0]

        # Collect all enabled providers:
        providers = [row[1] for row in self._model if row[0]]

        # Check if we can just subsitute "all" (add the 2 special providers)
        if len(providers) == len(plyr.PROVIDERS[self._chosen_type]['providers']) + 2:
            providers = ['all']

        # Remember the selection in the config:
        g.config.set(
            'metadata.providers.{t}'.format(t=self._chosen_type),
            providers
        )


def listbox_create_indented_labelrow(title, widget):
    row = listbox_create_labelrow(title, widget, bold=False)
    row.set_margin_left(10)
    return row


def create_section_label(title):
    label = Gtk.Label('<b>' + title + ':</b>')
    label.set_use_markup(True)
    label.set_alignment(0, 0.5)
    return label


###########################################################################
#                          Config Value Proxies                           #
###########################################################################


def config_proxy_bool(config_path):
    def _set_value(widget, _):
        g.config.set(config_path, widget.get_active())

    switch = Gtk.Switch()
    switch.set_active(g.config.get(config_path))
    switch.connect('notify::active', _set_value)
    return switch


def config_proxy_fraction(config_path, multiplier=1, label_format='{:2.3f}'):
    level_bar = LabeledLevelBar(
        multiplier=multiplier, label_format=label_format
    )

    def _set_value(widget, percent):
        level_bar.set_value(percent)
        g.config.set(config_path, percent)

    level_bar.set_value(g.config.get(config_path))
    level_bar.connect('changed', _set_value)
    return level_bar


def config_proxy_enum(config_path, values, active=None):
    list_store = Gtk.ListStore(str)
    for value in values:
        list_store.append([value])

    combo = Gtk.ComboBox.new_with_model(list_store)
    if active is not None:
        combo.set_active(active)

    renderer = Gtk.CellRendererText()
    combo.pack_start(renderer, True)
    combo.add_attribute(renderer, 'text', 0)

    def _set_value(combo):
        iterator = combo.get_active_iter()
        if iterator is not None:
            model = combo.get_model()
            g.config.set(config_path, model[iterator][0])

    value = g.config.get(config_path)
    try:
        idx = values.index(value)
        combo.set_active(idx)
    except ValueError:
        pass

    combo.connect('changed', _set_value)
    return combo


def config_proxy_range(config_path, min_value, max_value, step=1, digits=0):
    spin_button = Gtk.SpinButton.new_with_range(min_value, max_value, step)
    spin_button.set_digits(digits)

    def _set_value(spinbutton):
        g.config.set(config_path, spinbutton.get_value())

    spin_button.set_value(g.config.get(config_path))
    spin_button.connect('value-changed', _set_value)
    return spin_button


class SettingsSection(Section):
    def __init__(self):
        lsbox = Gtk.ListBox()

        Section.__init__(self, 'Finegrained Settings', lsbox)

        lsbox.set_selection_mode(Gtk.SelectionMode.NONE)
        lsbox.add(create_section_label('General Settings'))
        lsbox.add(
            listbox_create_indented_labelrow(
                'Quality/Speed ratio',
                config_proxy_fraction(
                    'metadata.qsratio',
                    label_format='{:0.2f}'
                ),
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Fuzzyness',
                config_proxy_fraction(
                    'metadata.fuzzyness',
                    multiplier=20,
                    label_format='{:2.0f}'
                )
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Normalization',
                config_proxy_enum(
                    'metadata.normalization',
                    ['aggressive', 'moderate', 'none'],
                    active=0
                )
            )
        )
        lsbox.add(create_section_label('Textrelated Settings'))
        lsbox.add(
            listbox_create_indented_labelrow(
                'Language',
                config_proxy_enum(
                    'metadata.language',
                    'en:de:fr:pl:es:it:jp:pt:ru:sv:tr:zh'.split(':'),
                    active=0
                )
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Only this language',
                config_proxy_bool('metadata.only_language')
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Force UTF-8 Encoding',
                config_proxy_bool('metadata.force_utf8')
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Maximal number of items per Provider',
                config_proxy_range('metadata.plugmax', 1, 100)
            )
        )
        lsbox.add(create_section_label('Imagerelated Settings'))
        lsbox.add(
            listbox_create_indented_labelrow(
                'Suggestion for minimal image size (in pixel)',
                config_proxy_range('metadata.image_min_size', -1, 1000)
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Suggestion for maximal image size (in pixel)',
                config_proxy_range('metadata.image_max_size', -1, 1000)
            )
        )
        lsbox.add(create_section_label('Downloading'))
        lsbox.add(
            listbox_create_indented_labelrow(
                'Timeout (in seconds)',
                config_proxy_range(
                    'metadata.timeout', 1, 200, digits=1, step=0.5
                )
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Parallel Downloads (per job)',
                config_proxy_range('metadata.parallel', 1, 20)
            )
        )


class DatabaseSection(Section):
    __gsignals__ = {
        'view-all-in-database': (
            GObject.SIGNAL_RUN_FIRST,
            None,
            (bool, int)
        )
    }

    def __init__(self):
        lsbox = Gtk.ListBox()
        lsbox.set_margin_top(5)

        Section.__init__(self, 'Database', lsbox)

        lsbox.set_selection_mode(Gtk.SelectionMode.NONE)

        button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        button_box.get_style_context().add_class(Gtk.STYLE_CLASS_LINKED)
        button_box.pack_start(
            Gtk.Label('View all database items'),
            True, True, 2
        )
        button_box.pack_start(Gtk.VSeparator(), False, False, 2)
        button_box.pack_start(
            Gtk.Image.new_from_icon_name(
                'applications-development',
                Gtk.IconSize.BUTTON
            ),
            False, False, 2
        )

        g.config.add_defaults({
            'gtk': {
                'metadata': {
                    'view_all': {
                        'limit_type': True,
                        'limit_num': -1
                    }
                }
            }
        })

        limit_type_swich = config_proxy_bool(
            'gtk.metadata.view_all.limit_type'
        )
        limit_num_button = config_proxy_range(
            'gtk.metadata.view_all.limit_num', -1, 32000
        )

        view_all_button = Gtk.Button()
        view_all_button.add(button_box)
        view_all_button.set_halign(Gtk.Align.END)
        view_all_button.connect(
            'clicked',
            lambda _: self.emit(
                'view-all-in-database',
                limit_type_swich.get_active(),
                int(limit_num_button.get_value())
            )
        )

        lsbox.add(
            listbox_create_indented_labelrow(
                '666 items in cache',
                view_all_button
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Amount to view (-1 is unlimited):',
                limit_num_button
            )
        )
        lsbox.add(
            listbox_create_indented_labelrow(
                'Limit to current type:',
                limit_type_swich
            )
        )


###########################################################################
#                           Put it all together                           #
###########################################################################


class SettingsChooser(Gtk.Grid):
    __gsignals__ = {
        'view-all-in-database': (
            GObject.SIGNAL_RUN_FIRST,
            None,
            (bool, int)
        )
    }

    def __init__(self):
        Gtk.Grid.__init__(self)

        self._provider_section = ProviderSection()
        self._database_section = DatabaseSection()

        # Forward signal:
        self._database_section.connect(
            'view-all-in-database',
            lambda _, get_type_only, amount: self.emit(
                'view-all-in-database',
                get_type_only,
                amount
            )
        )

        self.attach(SettingsSection(), 0, 0, 1, 1)
        self.attach(self._provider_section, 0, 1, 1, 1)
        self.attach(self._database_section, 0, 2, 1, 1)

    ###################
    #  Proxy Methods  #
    ###################

    def update_provider(self, get_type):
        self._provider_section.update_provider(get_type)

if __name__ == '__main__':
    # Internal:
    from moosecat.gtk.runner import main

    with main(metadata=True) as win:
        scw = Gtk.ScrolledWindow()
        scw.add(SettingsChooser())
        win.add(scw)
        win.set_default_size(450, 650)
