from moosecat.plugin_sys import psys
psys.scan()
print(psys._tag_list)
print(psys._module_list)

print('Load plugin glyr')
psys.load('glyr')
psys.load('lastfm')
print(psys._loaded_list)
print(psys['metadata'])

print('Unload plugin glyr')
psys.unload('glyr')
print(psys._loaded_list)

print('Load plugins by tag metadata')
psys.load_by_tag('metadata')
print(psys._loaded_list)
print(psys('metadata'))

print('Unload plugins by tag metadata')
psys.unload_by_tag('metadata')
print(psys._loaded_list)
