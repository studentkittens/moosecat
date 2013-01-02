

tmp = {'tag': {'modules': [1, 2, 3] }}

for m in tmp.get('tag', [])['modules']:
    print(m)

for m in tmp.get('notag', [])['modules']:
    print(m)
