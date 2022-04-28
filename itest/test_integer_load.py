import asyncio
import functools
import random
import time
import string
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


class TestIntegerLoad(TestBase):
    title = 'Test inserts and response'

    GEN_POINTS = functools.partial(
        gen_points,
        tp=int,
        mi=-2**10,
        ma=2**10,
        n=5,
        time_precision=TIME_PRECISION,
        ts_gap=3)

    async def select(self, client, series, n):
        series = {s.name: s for s in series}
        chars=string.ascii_letters + string.digits
        for _ in range(n):
            regex = ''.join(random.choice(chars) for _ in range(3))
            res = await client.query(f'select max() prefix "max-", min() prefix "min-", mean() prefix "mean-" from /.*{regex}.*/i')
            if res:
                print(res)
            # for s, p in res.items():
            #     self.assertEqual(sorted(series[s].points), p)
            await asyncio.sleep(0.2)

    async def insert(self, client, series, n):
        for _ in range(n):
            await client.insert_some_series(
                series, n=0.04, points=self.GEN_POINTS)
            await asyncio.sleep(0.2)

    @default_test_setup(
            1,
            time_precision=TIME_PRECISION,
            compression=True,
            auto_duration=True,
            optimize_interval=20)
    async def run(self):
        await self.client0.connect()

        series = gen_series(n=1000)

        insert = asyncio.ensure_future(
            self.insert(self.client0, series, n=50000))
        select = asyncio.ensure_future(
            self.select(self.client0, series, n=50000))
        await asyncio.gather(insert, select)

        self.client0.close()
        await asyncio.sleep(1800)


if __name__ == '__main__':
    random.seed(1)
    parse_args()
    run_test(TestIntegerLoad())
