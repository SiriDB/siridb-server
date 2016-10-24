'''Help module.

Build help structure for SiriDB grammar.

:copyright: 2016, Jeroen van der Heijden (Transceptor Technology)
'''

import os
import re
import sys

#
# Created with ls -A1
#
help_files = [f[:-3] for f in os.listdir('../help') if f.endswith('.md')]
help_files.sort()

def _build_structure(help_files):
    '''Return a tree structure for the help files.'''
    _structure = {}

    def _walk(d, keys, i):
        i += 1
        k = tuple(keys[:i])
        if k not in d:
            d[k] = {}
        if i < len(keys):
            _walk(d[k], keys, i)
    for help_file in help_files:
        _walk(_structure, help_file.split('_'), 0)
    return _structure


help_structure = _build_structure(help_files)


if __name__ == '__main__':
    print(help_structure)