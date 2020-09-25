import re
import random
import asyncio
import functools
import random
import time
import os
import sys
from math import nan
from testing import default_test_setup
from testing import parse_args
from testing import run_test
from testing import Server
from testing import SiriDB
from testing import TestBase
from testing.constants import PYGRAMMAR_PATH
from querygenerator.querygenerator import QueryGenerator
from querygenerator.k_map import k_map
sys.path.append(PYGRAMMAR_PATH)
from grammar import SiriGrammar  # nopep8


def gen_simple_data(m, n):
    series = {
        str(a).zfill(6): [[b*n+a, random.randint(0, 20)] for b in range(n)]
        for a in range(m)}
    return series


def update_k_map_show(show):
    kv = {a['name']: a['value'] for a in show['data']}
    k_map['r_integer']['k_active_handles'] = kv['active_handles']
    k_map['r_doubleq_str']['k_buffer_path'] = '"'+kv['buffer_path']+'"'
    k_map['r_integer']['k_buffer_size'] = kv['buffer_size']
    k_map['r_doubleq_str']['k_dbpath'] = '"'+kv['dbpath']+'"'
    k_map['r_float']['k_drop_threshold'] = kv['drop_threshold']
    k_map['r_doubleq_str']['k_ip_support'] = '"'+kv['ip_support']+'"'
    k_map['r_doubleq_str']['k_libuv'] = '"'+kv['libuv']+'"'
    k_map['r_integer']['k_max_open_files'] = kv['max_open_files']
    k_map['r_integer']['k_mem_usage'] = kv['mem_usage']
    k_map['r_integer']['k_open_files'] = kv['open_files']
    k_map['r_integer']['k_pool'] = kv['pool']
    k_map['r_integer']['k_received_points'] = kv['received_points']
    k_map['r_uinteger']['k_list_limit'] = kv['list_limit']
    k_map['r_integer']['k_startup_time'] = kv['startup_time']
    k_map['r_doubleq_str']['k_status'] = '"'+kv['status']+'"'
    k_map['r_doubleq_str']['k_sync_progress'] = '"'+kv['sync_progress']+'"'
    k_map['r_doubleq_str']['k_timezone'] = '"'+kv['timezone']+'"'
    k_map['r_integer']['k_uptime'] = kv['uptime']
    k_map['r_uuid_str']['r_uuid_str'] = kv['uuid']
    k_map['r_doubleq_str']['k_server'] = '"'+kv['server']+'"'
    k_map['r_doubleq_str']['uuid'] = '"'+kv['server']+'"'
    k_map['r_doubleq_str']['k_version'] = '"'+kv['version']+'"'
    k_map['r_uinteger']['k_port'] = kv['server'].split(':', 1)[1]
    k_map['r_uinteger']['k_select_points_limit'] = \
        kv['select_points_limit']
    k_map['r_doubleq_str']['k_reindex_progress'] = \
        '"'+kv['reindex_progress']+'"'


class TestGrammar(TestBase):
    title = 'Test from grammar'

    async def test_create_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }})
        for q in qb.generate_queries('create_stmt'):
            await self.client0.query(q)

    async def test_select_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': '',
                'k_filter': '',  # skip because only number type series
                'k_prefix': '',  # skip
                'k_suffix': '',  # skip
                'k_merge': '',  # skip
                'k_where': '',  # skip
                'after_expr': '',  # skip
                'before_expr': '',  # skip
                'between_expr': '',  # skip
                }
            })
        for q in qb.generate_queries('select_stmt'):
            await self.client0.query(q)

    async def test_revoke_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }})
        for q in qb.generate_queries('revoke_stmt'):
            await self.client0.query(q)

    async def test_grant_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }})
        for q in qb.generate_queries('grant_stmt'):
            await self.client0.query(q)

    async def test_alter_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': '', 
                'k_now': '',  # not possible (set expiration num/log)
                'set_name': '',  # skip
                'set_address': '',  # not possible
                'set_port': '',  # not possible
                'set_timezone': '',  # same value error
                'set_log_level': '',  # not required, but skip to keep log level
            }})
        for q in qb.generate_queries('alter_stmt'):
            await self.client0.query(q)

    async def test_count_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }})
        for q in qb.generate_queries('count_stmt'):
            await self.client0.query(q)

    async def test_list_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }})
        for q in qb.generate_queries('list_stmt'):
            await self.client0.query(q)

    async def test_drop_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': '',
                'drop_server': '',   # not possible
            }})
        for q in qb.generate_queries('drop_stmt'):
            await self.client0.query(q)

    async def test_show_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }})
        for q in qb.generate_queries('show_stmt'):
            await self.client0.query(q)

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        # await self.db.add_pool(self.server1, sleep=2)

        update_k_map_show(await self.client0.query('show'))

        series = gen_simple_data(20, 70)

        await self.client0.insert(series)
        await self.client0.query('create group `GROUP_OR_TAG` for /00000.*/')
        
        await self.test_create_stmt()

        time.sleep(2)

        await self.test_count_stmt()

        await self.test_list_stmt()

        await self.test_select_stmt()

        await self.test_revoke_stmt()

        await self.test_grant_stmt()

        await self.test_alter_stmt()

        await self.test_drop_stmt()

        await self.test_show_stmt()

        self.client0.close()

        return False

class TestGrammarStart(TestBase):

    async def test_all_stmts(self, client):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': '',
                'r_comment': '',
                'k_timeit': '',
                'select_stmt': '',
                'list_stmt': '',
                'count_stmt': '',

                'alter_group': '',
                #'drop_group': '',
                'alter_server': '',
                'drop_server': '',
                'alter_user': '',
                'drop_user': '',

                #'set_address': '',
                #'set_port': '',
                'set_timezone': '',
                'set_log_level': '',  # not required, skip to keep log level
                'set_expiration_num': '',
                'set_expiration_log': '',

                'k_prefix': '',
                'k_suffix': '',
                'k_filter': '',
                #'k_where': '',
                'after_expr': '',
                'before_expr': '',
                'between_expr': '',
                'k_merge': '',
        }})
        for q in qb.generate_queries():
            await self.client0.query(q)

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        # await self.db.add_pool(self.server1, sleep=2)

        update_k_map_show(await self.client0.query('show'))

        series = gen_simple_data(20, 70)

        await self.client0.insert(series)
        await self.client0.query('create group `GROUP_OR_TAG` for /00000.*/')
        #time.sleep(2)
        await self.test_all_stmts()
        self.client0.close()
        return False


if __name__ == '__main__':
    parse_args()
    run_test(TestGrammar())
