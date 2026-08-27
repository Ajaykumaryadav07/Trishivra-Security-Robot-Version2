import glob
import os
import sys

base = os.path.join(os.path.dirname(__file__), 'python')
paths = glob.glob(os.path.join(base, '*.py'))
failed = False
for path in paths:
    try:
        with open(path, 'r', encoding='utf-8') as f:
            f.read()
    except Exception as e:
        failed = True
        print(path, type(e).__name__, e)
if not failed:
    print('OK')
