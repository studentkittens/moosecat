import sys


class PluginSystem:
    def __init__(self):
        self._module_list = {}
        self._tag_list = {}
        self._loaded_list = {}

    def scan(self):
        import moosecat.plugins

    def register_tag(self, tag_description):
        tag_name = tag_description['name']
        if tag_name not in self._tag_list:
            self._tag_list[tag_name] = tag_description
            self._tag_list[tag_name]['modules'] = []

    def register_plugin(self, name, import_path, version, priority=50, on_load_func=None):

        if name in self._module_list:
            raise Exception("Error while register plugin (name already exists)")

        try:
            __import__(import_path)
            module = sys.modules[import_path]
        except(KeyError, ImportError):
            raise Exception("Error while register plugin (import)")

        for tag in self._tag_list:
            insert = []
            check = 0
            for method in self._tag_list[tag]['methods']:
                if hasattr(module, method):
                    check = check + 1
            if check == len(self._tag_list[tag]['methods']):
                insert.append(tag)

        if not insert:
            raise Exception("Error while register plugin (tag can't guessed)")

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
        if name not in self._loaded_list:
            if name in self._module_list:
                if self._module_list[name]['on_load_func']:
                    self._module_list[name]['on_load_func']()
                self._loaded_list[name] = True

    def unload(self, name):
        if name in self._loaded_list:
            del self._loaded_list[name]

    def load_by_tag(self, tag_name):
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                self.load(module[0])

    def unload_by_tag(self, tag_name):
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                self.unload(module[0])

    def __call__(self, tag_name):
        result = []
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                if module[0] in self._loaded_list:
                    result.append(self._module_list[module[0]]['module'])
        return result

    def __getitem__(self, tag_name):
        if tag_name in self._tag_list:
            for module in self._tag_list[tag_name]['modules']:
                if module[0] in self._loaded_list:
                    return self._module_list[module[0]]['module']

psys = PluginSystem()
