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

# HELP_FILES = [
#     'help',
#     'help_access',
#     'help_alter',
#     'help_alter_database',
#     'help_alter_group',
#     'help_alter_network',
#     'help_alter_series',
#     'help_alter_server',
#     'help_alter_user',
#     'help_continue',
#     'help_count',
#     'help_count_groups',
#     'help_count_networks',
#     'help_count_pools',
#     'help_count_series',
#     'help_count_servers',
#     'help_count_shards',
#     'help_count_users',
#     'help_create',
#     'help_create_group',
#     'help_create_network',
#     'help_create_user',
#     'help_drop',
#     'help_drop_group',
#     'help_drop_network',
#     'help_drop_series',
#     'help_drop_server',
#     'help_drop_shard',
#     'help_drop_user',
#     'help_functions',
#     'help_grant',
#     'help_list',
#     'help_list_groups',
#     'help_list_networks',
#     'help_list_pools',
#     'help_list_series',
#     'help_list_servers',
#     'help_list_shards',
#     'help_list_users',
#     'help_noaccess',
#     'help_pause',
#     'help_revoke',
#     'help_select',
#     'help_show',
#     'help_timeit',
#     'help_timezones'
# ]


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