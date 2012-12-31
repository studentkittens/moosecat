#!/usr/bin/env python
# encoding: utf-8
# Oben besser immer ein shebang + encoding hin (man weiß nie...)
# Für ultisnips: #!<tab>

import sys


'''
Docstrings!
'''
# Und mehr Kommentare!


class PluginSystem:
    def __init__(self):
        self._module_list = {}
        self._tag_list = {}
        self._loaded_list = {}

    def scan(self):
        # Hier vlt. __import__ nutzen
        # Ist vlt. klarer (und python-mode meckert)
        import moosecat.plugins

    def register_tag(self, tag_description):
        tag_name = tag_description['name']
        if tag_name not in self._tag_list:
            self._tag_list[tag_name] = tag_description
            self._tag_list[tag_name]['modules'] = []

    # Nachträgliche Anmerkung: Vlt. sollte man auch eine on_unload_func adden.
    def register_plugin(self, name, import_path, version, priority=50, on_load_func=None):
        # Keine generischen Exceptions nehmen.
        # Entweder eigene nehmen oder eine passende aus der stdlib
        # (preferred)

        if name in self._module_list:
            raise Exception("Error while register plugin (name already exists)")

        try:
            # Äh. Tipp: __import__ gibt auch was zurück :-)
            __import__(import_path)
            module = sys.modules[import_path]
        # Warum der KeyError hier? Was geht bei [] schief?
        except(KeyError, ImportError):
            # Again: Keine generischen Exceptions.
            raise Exception("Error while register plugin (import)")

        # "insert" -> "matched_tags", ich hatte einen starken Anfall debiler Verwirrung hier.
        # Wieso ist überhaupt insert IN der schleife?
        insert = []
        for tag in self._tag_list:
            check = 0
            for method in self._tag_list[tag]['methods']:
                if hasattr(module, method):
                    check = check + 1
            if check == len(self._tag_list[tag]['methods']):
                insert.append(tag)

        # Generische Exceptions. Äh äh.
        # Vlt. auch besser: if len(insert) not 0: (finde ich persönlich
        # sprechender, aber so gehts natürlich auch)
        if not insert:
            raise Exception("Error while register plugin (tag can't be guessed)")

        # Ist ['modules'] eigentlich nötig?
        # Werden da noch andere keys eingepflegt?
        # setdefault() könnte hier helfen.
        # Warum eigentlich der sort?
        for tag in insert:
            self._tag_list[tag]['modules'].append((name, priority))
            self._tag_list[tag]['modules'] = sorted(self._tag_list[tag]['modules'],
                                                    key=lambda module: module[1],
                                                    reverse=True)

        self._module_list[name] = {'name': name,
                                   'import_path': import_path,
                                   'version': version,
                                   'priority': priority,
                                   'on_load_func': on_load_func,
                                   'module': module
                                   }

    def load(self, name):
        # Fehlerbehandlung?
        if name not in self._loaded_list:
            if name in self._module_list:
                if self._module_list[name]['on_load_func']:
                    self._module_list[name]['on_load_func']()
                self._loaded_list[name] = True

    def unload(self, name):
        # Hier später analog zu load() noch on_unload_func aufrufen
        if name in self._loaded_list:
            del self._loaded_list[name]

    def load_by_tag(self, tag_name):
        # Okay. Statt dem if tag_name in self._tag_list ginge auch sowas:
        # for module in self._tag_list.get(tag_name, [])['modules']:
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                self.load(module[0])

    def unload_by_tag(self, tag_name):
        # Okay. (Siehe oben)
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                self.unload(module[0])

    def __call__(self, tag_name):
        # Für den Lerneffekt: Das geht auch kürzer :-)
        #
        # module_names = [m[0] for m in self._tag_list.get(tag_name, [])]
        # return list(filter(lambda m: m in self._loaded_list, module_names))
        # Da diese funktion später oft aufgerufen wird sollte man hier
        # möglichst "wenig" tun.

        result = []
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                if module[0] in self._loaded_list:
                    result.append(self._module_list[module[0]]['module'])
        return result

    def __getitem__(self, tag_name):
        # Duplicate zu __call__ eigentlich (bis auf das sofort return)
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                if module[0] in self._loaded_list:
                    return self._module_list[module[0]]['module']

# Hier kann man eigentlich dann gleich psys.scan() aufrufen, muss der caller
# das nimmer machen.
psys = PluginSystem()
