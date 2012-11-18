#!/usr/bin/env python
# encoding: utf-8
import importlib
import traceback


APIS = {
        'metadata': ['fetch', 'cancel_all'],
        'gtk_browser': ['get_window']
}


M = {}


def register(path, apis, version='0.1', priority=100):
    try:
        mod = importlib.import_module(path)
    except:
        traceback.print_exc()
    else:
        for api in apis:
            module_def = APIS.get(api)
            if module_def is None:
                print('No such API:', api)
            else:
                for func in module_def:
                    if not hasattr(mod, func):
                        print('Satellite', path, 'has no function', func)
                        return

            if M.get(api) is None:
                M[api] = [mod]
            else:
                M[api].append(mod)


if __name__ == '__main__':
    register('sub.glyr', ['metadata'])
    print(M)
