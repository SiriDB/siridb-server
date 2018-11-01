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


TIME_PRECISION = 'ms'


class TestCompression(TestBase):
    title = 'Test compression'

    GEN_POINTS = functools.partial(
        gen_points, n=100, time_precision=TIME_PRECISION)

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
            {'columns': ['name', 'length', 'type', 'start', 'end'],
                'series': [[
                    'series float',
                    10000, 'float',
                    self.series_float[0][0],
                    self.series_float[-1][0]], [
                        'series int',
                        10000, 'integer',
                        self.series_int[0][0],
                        self.series_int[-1][0]],
                ]})

    @default_test_setup(
        1,
        time_precision=TIME_PRECISION,
        optimize_interval=500,
        compression=True)
    async def run(self):
        await self.client0.connect()

        self.series_float = gen_points(
            tp=float, n=10000, time_precision=TIME_PRECISION, ts_gap='5m')
        random.shuffle(self.series_float)
        self.series_int = gen_points(
            tp=int, n=10000, time_precision=TIME_PRECISION, ts_gap='5m')
        random.shuffle(self.series_int)

        self.assertEqual(
            await self.client0.insert({
                'series float': self.series_float,
                'series int': self.series_int
            }), {'success_msg': 'Successfully inserted 20000 point(s).'})

        self.series_float.sort()
        self.series_int.sort()

        await self._test_series(self.client0)

        await self.client0.query('drop series /.*/ set ignore_threshold true')

        # Create some random series and start 25 insert task parallel
        series = gen_series(n=40)

        for i in range(40):
            await self.client0.insert_some_series(
                    series,
                    n=0.8,
                    timeout=0,
                    points=self.GEN_POINTS)

        # Check the result
        await self.assertSeries(self.client0, series)

        for i in range(40):
            await self.client0.insert_some_series(
                    series,
                    n=0.8,
                    timeout=0,
                    points=self.GEN_POINTS)

        # Check the result
        await self.assertSeries(self.client0, series)

        self.client0.close()

        result = await self.server0.stop()
        self.assertTrue(result)

        await self.server0.start(sleep=20)
        await self.client0.connect()

        # Check the result after rebooting the server
        await self.assertSeries(self.client0, series)


if __name__ == '__main__':
    random.seed(1)
    parse_args()
    run_test(TestCompression())
