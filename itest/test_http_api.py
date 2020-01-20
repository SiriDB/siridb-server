import requests
import json
from testing import gen_points
import asyncio
import functools
import random
import time
import math
import re
import qpack
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
from testing import parse_args

TIME_PRECISION = 's'


class TestHTTPAPI(TestBase):
    title = 'Test HTTP API requests'

    @default_test_setup(3, time_precision=TIME_PRECISION)
    async def run(self):
        await self.client0.connect()

        x = requests.get(
            f'http://localhost:9020/get-version', auth=('sa', 'siri'))

        self.assertEqual(x.status_code, 200)
        v = x.json()
        self.assertTrue(isinstance(v, list))
        self.assertTrue(isinstance(v[0], str))

        x = requests.post(
            f'http://localhost:9020/insert/dbtest',
            auth=('iris', 'siri'),
            headers={'Content-Type': 'application/json'})

        self.assertEqual(x.status_code, 400)

        series_float = gen_points(
            tp=float, n=10000, time_precision=TIME_PRECISION, ts_gap='5m')

        series_int = gen_points(
            tp=int, n=10000, time_precision=TIME_PRECISION, ts_gap='5m')

        data = {
            'my_float': series_float,
            'my_int': series_int
        }

        x = requests.post(
            f'http://localhost:9020/insert/dbtest',
            data=json.dumps(data),
            auth=('iris', 'siri'),
            headers={'Content-Type': 'application/json'}
        )

        self.assertEqual(x.status_code, 200)
        self.assertDictEqual(x.json(), {
            'success_msg': 'Successfully inserted 20000 point(s).'})

        data = {
            'dbname': 'dbtest',
            'host': 'localhost',
            'port': 9000,
            'username': 'iris',
            'password': 'siri'
        }

        x = requests.post(
            f'http://localhost:9021/new-pool',
            data=json.dumps(data),
            auth=('sa', 'siri'),
            headers={'Content-Type': 'application/json'})

        self.assertEqual(x.status_code, 200)
        self.assertEqual(x.json(), 'OK')

        self.db.servers.append(self.server1)
        await self.assertIsRunning(self.db, self.client0, timeout=30)

        data = {'data': [[1579521271, 10], [1579521573, 20]]}
        x = requests.post(
            f'http://localhost:9020/insert/dbtest',
            json=data,
            auth=('iris', 'siri'))

        self.assertEqual(x.status_code, 200)
        self.assertDictEqual(x.json(), {
            'success_msg': 'Successfully inserted 2 point(s).'})

        x = requests.post(
            f'http://localhost:9020/query/dbtest',
            json={'q': 'select * from "data"'},
            auth=('iris', 'siri'))

        self.assertEqual(x.status_code, 200)
        self.assertEqual(x.json(), data)

        x = requests.post(
            f'http://localhost:9020/query/dbtest',
            json={'q': 'select * from "data"', 't': 'ms'},
            auth=('iris', 'siri'))

        data = {
            'data': [[p[0] * 1000, p[1]] for p in data['data']]
        }

        self.assertEqual(x.status_code, 200)
        self.assertEqual(x.json(), data)

        x = requests.post(
            f'http://localhost:9020/query/dbtest',
            data=qpack.packb({
                'q': 'select sum(1579600000) from "data"',
                't': 'ms'}),
            headers={'Content-Type': 'application/qpack'},
            auth=('iris', 'siri'))

        self.assertEqual(x.status_code, 200)
        self.assertEqual(
            qpack.unpackb(x.content, decode='utf8'),
            {'data': [[1579600000000, 30]]})

        x = requests.post(
            f'http://localhost:9021/new-account',
            json={'account': 't', 'password': ''},
            auth=('sa', 'siri'))

        self.assertEqual(x.status_code, 400)
        self.assertEqual(x.json(), {'error_msg':
                'service account name should have at least 2 characters'})

        x = requests.post(
            f'http://localhost:9021/new-account',
            json={'account': 'tt', 'password': 'pass'},
            auth=('sa', 'siri'))

        self.assertEqual(x.status_code, 200)

        data = {
            'dbname': 'dbtest',
            'host': 'localhost',
            'port': 1234,
            'pool': 0,
            'username': 'iris',
            'password': 'siri'
        }

        auth = ('tt', 'pass')
        x = requests.post(
            f'http://localhost:9021/new-replica', json=data, auth=auth)

        self.assertEqual(x.status_code, 400)
        self.assertEqual(x.json(), {
            'error_msg': "database name already exists: 'dbtest'"})

        x = requests.post(
            f'http://localhost:9022/new-replica', json=data, auth=auth)
        self.assertEqual(x.status_code, 401)

        auth = ('sa', 'siri')
        x = requests.post(
            f'http://localhost:9022/new-replica', json=data, auth=auth)

        self.assertEqual(x.status_code, 400)
        self.assertEqual(x.json(), {
            'error_msg':
                "connecting to server 'localhost:1234' failed with error: "
                "connection refused"})

        data['port'] = 9000
        x = requests.post(
            f'http://localhost:9022/new-replica', json=data, auth=auth)
        self.assertEqual(x.status_code, 200)
        self.assertEqual(x.json(), 'OK')

        self.db.servers.append(self.server2)
        await self.assertIsRunning(self.db, self.client0, timeout=30)

        x = requests.get(
            f'http://localhost:9022/get-databases', auth=auth)
        self.assertEqual(x.status_code, 200)
        self.assertEqual(x.json(), ['dbtest'])

        self.client0.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestHTTPAPI())

