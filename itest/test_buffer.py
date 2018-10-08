import os
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
from testing import parse_args


class TestBuffer(TestBase):
    title = 'Test buffer object'

    async def _add_points(self):
        for series_name in ['iris', 'db', 'ligo', 'sasha']:
            if series_name not in self.total:
                self.total[series_name] = []
            batches = sum([ord(c) for c in series_name]) % 100
            for i in range(batches):
                npoints = []
                n = int(i**0.5 * 10000 % 5) + 1
                for p in range(n):
                    self.ts += (n + 5000) if i % 2 else (n - 5000)
                    npoints.append([self.ts, i*1000+p])
                self.total[series_name].extend(npoints)
                self.total[series_name].sort()
                await self.client0.insert({series_name: npoints})

    async def _test_equal(self):
        for series_name, points in self.total.items():
            res = await self.client0.query(f'select * from "{series_name}"')
            res = res[series_name]
            self.assertEqual(len(points), len(res))
            self.assertEqual(points, res)

    async def _change_buf_size(self, buffer_size):
        self.client0.close()
        result = await self.server0.stop()
        self.assertTrue(result)
        self.server0.set_buffer_size(self.db, buffer_size)
        await self.server0.start(sleep=20)
        await self.client0.connect()
        res = await self.client0.query('show buffer_size')
        self.assertEqual(res['data'][0]['value'], buffer_size)
        await self._test_equal()

    async def _change_buf_path(self, buffer_path):
        self.client0.close()
        result = await self.server0.stop()
        self.assertTrue(result)
        self.server0.set_buffer_path(self.db, buffer_path)
        await self.server0.start(sleep=20)
        await self.client0.connect()
        res = await self.client0.query('show buffer_path')
        self.assertEqual(res['data'][0]['value'], buffer_path)
        res = await self.client0.query('show open_files')
        self.assertEqual(res['data'][0]['value'], 3)
        res = await self.client0.query(
            f'alter server {self.uuid} set backup_mode true')
        await asyncio.sleep(5)
        res = await self.client0.query('show open_files')
        self.assertEqual(res['data'][0]['value'], 0)
        res = await self.client0.query(
            f'alter server {self.uuid} set backup_mode false')
        await self._test_equal()

    @default_test_setup(1, time_precision='s', compression=False)
    async def run(self):
        await self.client0.connect()

        res = await self.client0.query('show uuid')
        self.uuid = res['data'][0]['value']

        self.ts = 1500000000
        self.total = {}

        await self._add_points()
        await self._test_equal()

        await self._change_buf_path(os.path.join(
            self.server0.dbpath,
            self.db.dbname,
            '../buf/'))

        await self._change_buf_size(8192)

        await self._add_points()
        await self._test_equal()

        await self._change_buf_size(8192)
        await self._change_buf_size(512)

        await self._add_points()
        await self._test_equal()

        await self._change_buf_size(1024)

        await self._change_buf_path(os.path.join(
            self.server0.dbpath,
            self.db.dbname,
            'buf/'))

        return False


if __name__ == '__main__':
    parse_args()
    run_test(TestBuffer())
