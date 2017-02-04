import sys
import json

wrapper_file = open(sys.argv[1], 'r+')

line = '#'
while line.startswith('#'):
    pos = wrapper_file.tell()
    line = wrapper_file.readline()
    
wrapper_file.seek(pos)

wrapper = json.loads(wrapper_file.read())

for i, item in enumerate(wrapper['captures']):
    wrapper['captures'][i]['stdout'] = wrapper['captures'][0]['stdout']
    wrapper['captures'][i]['stderr'] = wrapper['captures'][0]['stderr']

wrapper_file.seek(0)
wrapper_file.truncate()
wrapper_file.write(json.dumps(wrapper))
