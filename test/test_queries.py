import re
import random
import asyncio
import functools
import random
import time
from testing import Client
from testing import default_test_setup
from testing import gen_data
from testing import gen_points
from testing import gen_series
from testing import InsertError
from testing import PoolError
from testing import QueryError
from testing import run_test
from testing import Series
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError
from querybouwer.querybouwer import Querybouwer
from querybouwer.k_map import k_map
from querybouwer.validate import SiriRipOff, compare_result
from cmath import isfinite
from math import nan

M, N = 20, 70
DATA = {
    str(a).zfill(6):
        [[b*N+a, random.randint(0, 20)]
            for b in range(N)] for a in range(M)}

client_validate = SiriRipOff()
client_validate.groups = {'GROUP': re.compile('.*')}
client_validate.update(DATA)


class TestSelect(TestBase):
    title = 'Test select'

    async def test_main(self):
        qb = Querybouwer({
            'regex_map': k_map,
            'max_list_n_map': {
                'series_match': 1,
                'aggregate_functions': 2},
            'default_list_n': 2,
            'replace_map': {
                'k_prefix': '',
                'k_suffix': '',
                'k_where': '',
                'after_expr': '',
                'before_expr': '',
                'between_expr': '',
                'k_merge': '',
                'r_singleq_str': '',
                'k_now': '',
                'r_float': '',
                'r_time_str': '',
            }
        })

        for q in qb.generate_queries('select_stmt'):
            if '~' in q:
                continue
            if 'between now' in q:
                continue
            if "000000" in q or \
                    '1970' in q or \
                    '*' in q or \
                    '==' in q or \
                    '>=' in q or \
                    '<' in q:
                continue

            if 'filter' in q and 'derivative' in q.split('filter', 1)[0]:
                continue

            print(q)
            a, b = q.split('   , ', 1)
            q = a + ' suffix "a",' + b
            # a, b = q.split('   , ', 1)
            # q = a + ' suffix "b",'+b
            # q = 'select * from /.*/ '+q

            res0 = await self.client0.query(q)

    async def test_where(self):
        qb = Querybouwer({
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
            if '~' in q:
                continue
            if 'between now' in q:
                continue
            print(q)
            res0 = await self.client0.query(q)
            # res1 = client_validate.query(q)
            # self.assertFalse(compare_result(res0, res1))

    async def test_between(self):
        qb = Querybouwer({
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
            if '~' in q:
                continue
            if 'between now' in q:
                continue
            print(q)
            self.assertFalse(
                validate_query_result(q, await self.client0.query(q)))

    async def test_merge(self):
        qb = Querybouwer({
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

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        for k, v in sorted(DATA.items()):
            await self.client0.insert({k: v})
        await self.client0.query('create group `GROUP` for /.*/')
        time.sleep(2)

        await self.test_main()
        # await self.test_where()
        # await self.test_between()
        # await self.test_merge()

        self.client0.close()

        return False


class TestAll(TestBase):

    title = 'Test test'

    async def update_k_map(self):
        res = await self.client0.query('show')
        kv = {a['name']: a['value'] for a in res['data']}
        # for k, v in sorted(kv.items()):
        #     print(
        #         'k_'+k in k_map['r_doubleq_str'] or
        #         'k_'+k in k_map['r_integer'] or
        #         'k_'+k in k_map['r_float'], k, v)
        k_map['r_integer']['k_active_handles'] = kv['active_handles']
        k_map['r_doubleq_str']['k_buffer_path'] = \
            '"{buffer_path}"'.format_map(kv)
        k_map['r_integer']['k_buffer_size'] = kv['buffer_size']
        k_map['r_doubleq_str']['k_dbpath'] = '"{dbpath}"'.format_map(kv)
        k_map['r_float']['k_drop_threshold'] = kv['drop_threshold']
        k_map['r_doubleq_str']['k_ip_support'] = \
            '"{ip_support}"'.format_map(kv)
        k_map['r_doubleq_str']['k_libuv'] = '"{libuv}"'.format_map(kv)
        k_map['r_integer']['k_max_open_files'] = kv['max_open_files']
        k_map['r_integer']['k_mem_usage'] = kv['mem_usage']
        k_map['r_integer']['k_open_files'] = kv['open_files']
        k_map['r_integer']['k_pool'] = kv['pool']
        k_map['r_integer']['k_received_points'] = kv['received_points']
        k_map['r_doubleq_str']['k_reindex_progress'] = \
            '"{reindex_progress}"'.format_map(kv)
        k_map['r_integer']['k_startup_time'] = kv['startup_time']
        k_map['r_doubleq_str']['k_status'] = '"{status}"'.format_map(kv)
        k_map['r_doubleq_str']['k_sync_progress'] = \
            '"{sync_progress}"'.format_map(kv)
        k_map['r_doubleq_str']['k_timezone'] = '"{timezone}"'.format_map(kv)
        k_map['r_integer']['k_uptime'] = kv['uptime']
        k_map['r_uuid_str']['r_uuid_str'] = kv['uuid']
        k_map['r_doubleq_str']['k_server'] = '"{server}"'.format_map(kv)
        k_map['r_doubleq_str']['uuid'] = '"{server}"'.format_map(kv)
        k_map['r_doubleq_str']['k_version'] = '"{version}"'.format_map(kv)

    async def test_create_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map,
            'replace_map': {
                'r_singleq_str': ''
            }
        })

        for q in qb.generate_queries('create_stmt'):
            print(q)
            await self.client0.query(q)

    async def test_select_stmt(self):
        qb = Querybouwer({
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
            if '~' in q:
                continue
            if 'between now' in q:
                continue
            print(q)
            await self.client0.query(q)

    async def test_revoke_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('revoke_stmt'):
            print(q)
            await self.client0.query(q)

    async def test_grant_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('grant_stmt'):
            print(q)
            await self.client0.query(q)

    async def test_alter_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('alter_stmt'):
            if 'set address' in q:
                continue  # not possible
            if 'set port' in q:
                continue  # not possible
            if 'set timezone' in q:
                continue  # same as previous value error
            if 'set name' in q:
                continue  # same as previous value error
            print(q)
            await self.client0.query(q)

    async def test_count_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('count_stmt'):
            print(q)
            await self.client0.query(q)

    async def test_list_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('list_stmt'):
            print(q)
            await self.client0.query(q)

    async def test_drop_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('drop_stmt'):
            if 'drop server' in q:
                continue  # not possible
            if 'drop series' in q:
                continue
            print(q)
            await self.client0.query(q)

    async def test_show_stmt(self):
        qb = Querybouwer({
            'regex_map': k_map, 'replace_map': {'r_singleq_str': ''}})
        for q in qb.generate_queries('show_stmt'):
            print(q)
            await self.client0.query(q)

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        # await self.db.add_pool(self.server1, sleep=2)

        await self.update_k_map()

        await self.client0.insert(DATA)

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
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestAll())
    # run_test(TestSelect())
