import os
import sys

print('WARNING: This script does not support spaces in the path!')
target_dir = sys.argv[1]
for file in os.listdir(target_dir):
    if file.endswith('.whl'):
        cmd = 'pip install {}'.format(os.path.join(target_dir, file))
        print(cmd)
        os.system(cmd)