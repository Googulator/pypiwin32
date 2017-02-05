import sys
import json

wrapper_file = open(sys.argv[1], 'r+')

line = '#'
while line.startswith('#'):
    pos = wrapper_file.tell()
    line = wrapper_file.readline()
    
wrapper_file.seek(pos)

wrapper = json.load(wrapper_file)

for i, item in enumerate(wrapper['captures']):
    wrapper['captures'][i].setdefault('stdout', '')
    wrapper['captures'][i].setdefault('stderr', '')
    if not wrapper['captures'][i]['stdout']:
        wrapper['captures'][i]['stdout'] = wrapper['captures'][0]['stdout']
    if not wrapper['captures'][i]['stderr']:
        wrapper['captures'][i]['stderr'] = wrapper['captures'][0]['stderr']

wrapper_file.seek(0)
wrapper_file.truncate()
json.dump(wrapper, wrapper_file, indent=4)
