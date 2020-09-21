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
from testing import run_test
from testing import Server
from testing import SiriDB
from testing import TestBase
from testing import parse_args
from testing.constants import PYGRAMMAR_PATH
from querygenerator.querygenerator import QueryGenerator
from querygenerator.k_map import k_map
sys.path.append(PYGRAMMAR_PATH)
from grammar import SiriGrammar  # nopep8


def gen_simple_data(m, n):
    series = {
        str(a).zfill(6): [[b*n+a, random.randint(0, 20)] for b in range(n)]
        for a in range(m)}
    # series = {
    #   str(a).zfill(6): [[b, nan if b > 15 else float(random.randint(0, 20))]
    #   for b in range(n)] for a in range(m)}
    return series


def update_k_map_show(show):
    kv = {a['name']: a['value'] for a in show['data']}
    # for k, v in sorted(kv.items()):
    #     print('k_'+k in k_map['r_doubleq_str'] or
    #           'k_'+k in k_map['r_integer'] or
    #           'k_'+k in k_map['r_float'], k, v)
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
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('create_stmt'):
            await self.client0.query(q)

    async def test_select_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'max_list_n_map': {
                'series_match': 1,
                'aggregate_functions': 1},
            'replace_map': {
                'k_prefix': '',
                'k_suffix': '',
                'k_where': '',
                'after_expr': '',
                'before_expr': '',
                'between_expr': '',
                'k_merge': '',
                'r_singleq_str': '',
                }
            })
        for q in qb.generate_queries('select_stmt'):
            if '/' in q:
                # QueryError: Cannot use a string filter on number type.
                continue
            elif '~' in q:
                # QueryError: Cannot use a string filter on number type.
                continue
            elif 'between now' in q:
                continue
            await self.client0.query(q)

    async def test_revoke_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('revoke_stmt'):
            await self.client0.query(q)

    async def test_grant_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('grant_stmt'):
            await self.client0.query(q)

    async def test_alter_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('alter_stmt'):
            if 'set address' in q:
                continue  # kan niet
            if 'set port' in q:
                continue  # kan niet
            if 'set timezone' in q:
                continue  # zelfde as vorig waarde error
            # if 'set name' in q:
            #     continue  # zelfde as vorig waarde error
            if 'group' in q and 'name' in q:
                continue  # zelfde as vorig waarde error
            await self.client0.query(q)

    async def test_count_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('count_stmt'):
            await self.client0.query(q)

    async def test_list_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('list_stmt'):
            await self.client0.query(q)

    async def test_drop_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('drop_stmt'):
            if 'drop server' in q:
                continue  # kan niet
            if 'drop user' in q:
                continue  # user not exists err
            if 'drop series' in q:
                continue  # and not 'where' in q: continue
            await self.client0.query(q)

    async def test_show_stmt(self):
        qb = QueryGenerator(SiriGrammar, {
            'regex_map': k_map,
            'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('show_stmt'):
            await self.client0.query(q)

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        # await self.db.add_pool(self.server1, sleep=2)

        update_k_map_show(await self.client0.query('show'))

        series = gen_simple_data(20, 70)

        await self.client0.insert(series)
        # await self.client0.query('create group `GROUP` for /.*/')

        await self.test_create_stmt()

        time.sleep(2)

        await self.test_select_stmt()

        await self.test_revoke_stmt()

        await self.test_grant_stmt()

        await self.test_alter_stmt()

        await self.test_count_stmt()

        await self.test_list_stmt()

        await self.test_drop_stmt()

        await self.test_show_stmt()

        self.client0.close()

        return False


if __name__ == '__main__':
    parse_args()
    run_test(TestGrammar())
