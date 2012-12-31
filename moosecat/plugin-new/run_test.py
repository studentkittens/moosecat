from moosecat.plugin_sys import psys

from pprint import pprint

# Sieht soweit gut aus.

psys.scan()
pprint(psys._tag_list)
pprint(psys._module_list)

pprint('Load plugin glyr')
psys.load('glyr')
psys.load('lastfm')
pprint(psys._loaded_list)
pprint(psys['metadata'])

pprint('Unload plugin glyr')
psys.unload('glyr')
pprint(psys._loaded_list)

pprint('Load plugins by tag metadata')
psys.load_by_tag('metadata')
pprint(psys._loaded_list)
pprint(psys('metadata'))

pprint('Unload plugins by tag metadata')
psys.unload_by_tag('metadata')
pprint(psys._loaded_list)
