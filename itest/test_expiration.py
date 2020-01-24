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


TIME_PRECISION = 's'


class TestExpiration(TestBase):
    title = 'Test shard expiration'

    GEN_POINTS = functools.partial(
        gen_points, n=1, time_precision=TIME_PRECISION)

    async def _test_series(self, client):

        result = await client.query('select * from "series float"')
        self.assertEqual(result['series float'], self.series_float)

        result = await client.query('select * from "series int"')
        self.assertEqual(result['series int'], self.series_int)

        result = await client.query(
            'list series name, length, type, start, end')
        result['series'].sort()
        self.assertEqual(
            result,
            {
                'columns': ['name', 'length', 'type', 'start', 'end'],
                'series': [
                    [
                        'series float',
                        10000, 'float',
                        self.series_float[0][0],
                        self.series_float[-1][0]],
                    [
                        'series int', 10000,
                        'integer',
                        self.series_int[0][0],
                        self.series_int[-1][0]],
                ]
            })

    async def insert(self, client, series, n, timeout=1):
        for _ in range(n):
            await client.insert_some_series(
                series, timeout=timeout, points=self.GEN_POINTS)
            await asyncio.sleep(1.0)

    @default_test_setup(
        2,
        time_precision=TIME_PRECISION,
        compression=True,
        optimize_interval=20)
    async def run(self):
        await self.client0.connect()

        await self.db.add_replica(self.server1, 0, sleep=30)
        # await self.db.add_pool(self.server1, sleep=30)

        await self.assertIsRunning(self.db, self.client0, timeout=30)

        await self.client1.connect()

        self.series_float = gen_points(
            tp=float, n=10000, time_precision=TIME_PRECISION, ts_gap='10m')
        random.shuffle(self.series_float)

        self.series_int = gen_points(
            tp=int, n=10000, time_precision=TIME_PRECISION, ts_gap='10m')
        random.shuffle(self.series_int)

        self.assertEqual(
            await self.client0.insert({
                'series float': self.series_float,
                'series int': self.series_int
            }), {'success_msg': 'Successfully inserted 20000 point(s).'})

        self.series_float.sort()
        self.series_int.sort()

        await self._test_series(self.client0)

        total = (await self.client0.query('count shards'))['shards']
        rest = (
            await self.client0.query('count shards where end > now - 3w')
        )['shards']

        self.assertGreater(total, rest)

        await self.client0.query('alter database set expiration_num 3w')
        await asyncio.sleep(40)  # wait for optimize to complete

        total = (await self.client0.query('count shards'))['shards']
        self.assertEqual(total, rest)

        await self.client0.query('alter database set expiration_log 2w')
        await self.client0.insert({
            'series_log': [
                [int(time.time()) - 3600*24*15, "expired_log"]
            ]
        })

        res = await self.client0.query('list series name, length "series_log"')
        self.assertEqual(len(res['series']), 0)

        await self.client0.insert({
            'series_log': [
                [int(time.time()) - 3600*24*15, "expired_log"],
                [int(time.time()) - 3600*24*7, "valid_log"],
            ]
        })

        res = await self.client0.query('list series name, length "series_log"')
        self.assertEqual(len(res['series']), 1)
        self.assertEqual(res['series'], [['series_log', 1]])

        await self.client0.query('alter database set drop_threshold 0.1')

        with self.assertRaisesRegex(
                QueryError,
                "This query would drop .*"):
            result = await self.client0.query(
                'alter database set expiration_num 1w')

        total = (await self.client0.query('count shards'))['shards']
        rest = (
            await self.client0.query('count shards where end > now - 1w')
        )['shards']

        result = await self.client0.query(
                'alter database set expiration_num 1w '
                'set ignore_threshold true')

        await asyncio.sleep(40)  # wait for optimize to complete

        total = (await self.client0.query('count shards'))['shards']
        self.assertEqual(total, rest)

        self.client0.close()
        self.client1.close()


if __name__ == '__main__':
    random.seed(1)
    parse_args()
    run_test(TestExpiration())
