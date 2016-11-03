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

TIME_PRECISION = 's'


class TestInsert(TestBase):
    title = 'Test inserts and response'

    GEN_POINTS = functools.partial(gen_points, n=1, time_precision=TIME_PRECISION)

    async def _test_series(self, client):

        result = await client.query('select * from "series float"')
        self.assertEqual(result['series float'], self.series_float)

        result = await client.query('select * from "series int"')
        self.assertEqual(result['series int'], self.series_int)

        result = await client.query('list series name, length, type, start, end')
        result['series'].sort()
        self.assertEqual(
            result,
            {   'columns': ['name', 'length', 'type', 'start', 'end'],
                'series': [
                    ['series float', 10000, 'float', self.series_float[0][0], self.series_float[-1][0]],
                    ['series int', 10000, 'integer', self.series_int[0][0], self.series_int[-1][0]],
                ]
            })

    async def insert(self, client, series, n, timeout=1):
        for _ in range(n):
            await client.insert_some_series(series, timeout=timeout, points=self.GEN_POINTS)
            await asyncio.sleep(1.0)



    @default_test_setup(1, time_precision=TIME_PRECISION)
    async def run(self):
        await self.client0.connect()


        # Create some random series and start 25 insert task parallel
        series = gen_series(n=1000)
        tasks = [
            asyncio.ensure_future(
                self.client0.insert_some_series(
                    series,
                    timeout=0,
                    points=self.GEN_POINTS))
            for i in range(25)]

        await asyncio.gather(*tasks)

        await self.client0.query('list series /.*/ - /.*/')
        await self.client0.query('list series /.*/ | /.*/')
        await self.client0.query('list series /.*/ & /.*/')
        await self.client0.query('list series /.*/ ^ /.*/')

        await self.client0.query('list series /.*/ - /a.*/')
        await self.client0.query('list series /.*/ | /a.*/')
        await self.client0.query('list series /.*/ & /a.*/')
        await self.client0.query('list series /.*/ ^ /a.*/')

        await self.client0.query('list series /.*/ - /a.*/ | /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ | /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ | /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ | /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ | /.*/')
        await self.client0.query('list series /.*/ | /a.*/ | /.*/')
        await self.client0.query('list series /.*/ & /a.*/ | /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ | /.*/')

        await self.client0.query('list series /.*/ - /a.*/ - /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ - /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ - /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ - /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ - /.*/')
        await self.client0.query('list series /.*/ | /a.*/ - /.*/')
        await self.client0.query('list series /.*/ & /a.*/ - /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ - /.*/')

        await self.client0.query('list series /.*/ - /a.*/ & /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ & /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ & /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ & /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ & /.*/')
        await self.client0.query('list series /.*/ | /a.*/ & /.*/')
        await self.client0.query('list series /.*/ & /a.*/ & /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ & /.*/')

        await self.client0.query('list series /.*/ - /a.*/ ^ /b.*/')
        await self.client0.query('list series /.*/ | /a.*/ ^ /b.*/')
        await self.client0.query('list series /.*/ & /a.*/ ^ /b.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ ^ /b.*/')

        await self.client0.query('list series /.*/ - /a.*/ ^ /.*/')
        await self.client0.query('list series /.*/ | /a.*/ ^ /.*/')
        await self.client0.query('list series /.*/ & /a.*/ ^ /.*/')
        await self.client0.query('list series /.*/ ^ /a.*/ ^ /.*/')

        self.client0.close()

        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestInsert())
