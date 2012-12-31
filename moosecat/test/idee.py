'''
Allgemeine Anmerkungen:

* Das ganze sollte weniger auf Klassenbasis, als auf Modulsbasis sein.
* Vom Programm-Ablauf her (siehe auch Programmcode unten):

    * Warnung: ob das 100% so geht weiß ich nicht, aber
      zmd. gingen teile davon :-)
    * main.py importiert plugin_sys.py
    * Durch den Import wird das PluginSystem() instanziert,
      und kann fortan genutzt werden.
    * Bei der Instanzierung von PluginSystem() wird
      moosecat.plugins importiert. Da in plugins/ ein
      __init__.py liegt wird dieser ausgeführt.
      Darin stehen wiederum import statements die plugins verschiedener tags
      importieren. (code ist verständlicher als der text hoffe ich.)
    * danach sind alle vorhanden plugins und tags registriert.
    * main.py kann dann psys.load_by_tag('metadata') und dadurch werden die
      plugins mit diesem tag tatsächlich geladen.
    * Über "psys" können dann untereinander methoden aufgerufen werden.
    * Externe Plugins können durch "eggs" realisiert werden. (googlen)

Zum bisherigen Code.

* moduls -> modules.
* Die Tagbeschreibung sollte nicht verändert werden.
* Plugins und Tags sollten eigene Listen haben.
* Die Tagdescription ist nur ein "Vertrag". Er selbst hat keine Ahnung
  welche Plugins ihn erfüllen.


Folgendes "Beispiel" hat folgende Ordnerstruktur:

moosecat
├── plugin_sys.py
├── main.py
├── __init__.py
└── plugins
    ├── __init__.py
    └── metadata
        ├── glyr.py
        ├── __init__.py
        └── lastfm.py

2 directories, 7 files

'''

###########################################################################
#                           plugin_sys.py                                 #
###########################################################################


# Globale Instanz auf die alle anderen zugreifen dürfen
psys = PluginSystem()


class PluginSystem:
    def __init__(self):
        self._module_list = []
        self._tag_list = []

        # Gehe durch
        import moosecat.plugins

    def register_tag(self, tag_name, description):
        # ...
        # check stuff
        self._tag_list.append(tag_name)

    def register_plugin(self, name, import_path, version, priority=50, on_load_func=None):
        # ...
        try:
            # __import__ liefert ein module object zurück
            p_module = __import__(import_path)
        except ImportError:
            # ...
            pass

        # Anhand von p_module tags prüfen etc.
        # Evtl. wäre hier eine Plugin-Klasse nützlich die name, import_path
        # (etc.) und p_module kapselt.

    def load(self, name):
        pass

    def unload(self, name):
        pass

    def load_by_tag(self, tag_name):
        # gehe durch alle plugins für ein tag und lade sie
        pass

    def unload_by_tag(self, tag_name):
        # dasselbe
        pass

    def __call__(self, tag_name):
        '''
        Hier "get_all() implementieren

        So kann man sowas schreiben innerhalb eines Plugins:

            psys('metadata') -> Liste von für #metadata verfügbare module
        '''
        return some_modules

    def __getitem__(self, tag_name):
        '''
        Gier "get_first_one" implementieren

        So kann man sowas schreiben innerhalb eines Plugins:

            psys['metadata'].method1()

        Man kann direkt eine Methode method1() des am meisten priorisierten
        Plugins aufrufen
        '''
        return some_module



###########################################################################
#                                 main.py                                 #
###########################################################################


from moosecat.plugin_sys import psys

if __name__ == '__main__':
    # Some init ...
    # ...
    psys.load_by_tag('metadata')
    show(psys['metadata'].get_metadata('some_cover'))


###########################################################################
#              plugins/metadata/__init__.py                               #
###########################################################################

from moosecat.plugin_sys import psys

metadata_description = {
    'name': 'metadata',
    'description': 'Retrieve some fucking coverart and stuff',
    'methods': [get_metadata, close_all],
    'classes': [],
    'globals': []
}

# Register the tag
psys.register_tag(metadata_description)

# Register the single plugins in this directory
# Vielleicht kann man auch den import pfad automatisch ermitteln (TODO)
psys.register_plugin('glyr', 'moosecat.plugins.metadata.glyr', 1.0)

from .lastfm import on_load
psys.register_plugin('lastfm', 'moosecat.plugins.metadata.lastfm', 1.0, on_load_func=on_load)


###########################################################################
#                           plugins/__init__.py                           #
###########################################################################

# Make plugins/metadata/__init__.py called
import .metadata


###########################################################################
#                                 glyr.py                                 #
###########################################################################

# Ein metadata plugin für glyr

from moosecat.plugin_sys import psys

def get_metadata(some_args):
    return whatever


def close_all(some_args):
    pass


###########################################################################
#                                lastfm.py                                #
###########################################################################


def get_metadata(some_args):
    return what

def close_all(some_args):
    pass

# Bein laden des plugin aufgerufen, nicht beim registrieren.
# Das laden erfolgt nicht auf Wunsch des
def on_load():
    # on load kann für initialsierung genutzt werden,
    # zB für die Verbindung zu lastfm
    pass
