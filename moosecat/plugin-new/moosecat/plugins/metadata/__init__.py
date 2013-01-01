from moosecat.plugin_sys import psys

metadata_description = {'name': 'metadata',
                        'description': 'Retrieve some fucking coverart and stuff',
                        'methods': ['get_metadata', 'close_all'],
                        'classes': [],
                        'globals': []
                        }

psys.register_tag(metadata_description)
psys.register_plugin('glyr', 'moosecat.plugins.metadata.glyr', 1.0)
psys.register_plugin('lastfm', 'moosecat.plugins.metadata.lastfm', 1.0)
