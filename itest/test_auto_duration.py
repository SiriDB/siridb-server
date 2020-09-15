import asyncio
import functools
import random
import time
import math
import re
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


class TestAutoDuration(TestBase):
    title = 'Test select and aggregate functions'

    @default_test_setup(2, compression=False, buffer_size=1024)
    async def run(self):
        await self.client0.connect()

        await self.db.add_pool(self.server1, sleep=30)
        await self.assertIsRunning(self.db, self.client0, timeout=30)

        series = {}
        end_td = int(time.time())
        start_ts = end_td - (3600 * 24 * 7 * 10)
        tests = [[300, 10], [60, 5], [3600, 30], [60, 90], [10, 1]]

        for i, cfg in enumerate(tests):
            interval, r = cfg
            for nameval in [['int', 42], ['float', 3.14], ['str', 'hi']]:
                name, val = nameval
                series['{}-{}'.format(name, i)] = [
                    [t + random.randint(-r, r), val]
                    for t in range(start_ts, end_td, interval)
                ]

        self.assertEqual(
            await self.client0.insert(series),
            {'success_msg': 'Successfully inserted {} point(s).'.format(
                2484720)})

        self.client0.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestAutoDuration())
