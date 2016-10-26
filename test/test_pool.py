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


class TestPool(TestBase):
    title = 'Test pool object'

    async def insert(self, client, series, n, timeout=1):
        for _ in range(n):
            await client.insert_some_series(series, n=0.01, timeout=timeout)
            await asyncio.sleep(1.0)


    @default_test_setup(4)
    async def run(self):

        series = gen_series(n=10000)

        await self.client0.connect()

        task0 = asyncio.ensure_future(self.insert(
            self.client0,
            series,
            200))

        await asyncio.sleep(5)

        await self.db.add_pool(self.server1, sleep=3)

        with self.assertRaises(AssertionError):
            await self.db.add_pool(self.server2, sleep=3)

        await self.client1.connect()
        task1 = asyncio.ensure_future(self.insert(
            self.client1,
            series,
            150))

        await asyncio.sleep(5)

        await self.assertIsRunning(self.db, self.client0, timeout=200)

        await asyncio.sleep(5)

        await self.db.add_replica(self.server3, 1, sleep=3)
        await self.client3.connect()
        await self.assertIsRunning(self.db, self.client3, timeout=200)

        await asyncio.sleep(5)

        await self.db.add_pool(self.server2, sleep=3)
        await self.client2.connect()

        task2 = asyncio.ensure_future(self.insert(
            self.client2,
            series,
            100))

        await self.assertIsRunning(self.db, self.client0, timeout=600)

        await asyncio.wait_for(task0, None)
        await asyncio.wait_for(task1, None)
        await asyncio.wait_for(task2, None)

        await asyncio.sleep(1)

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)
        await self.assertSeries(self.client2, series)
        await self.assertSeries(self.client3, series)

        self.client0.close()
        self.client1.close()
        self.client2.close()
        self.client3.close()

        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'INFO'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = False
    Server.BUILDTYPE = 'Debug'
    run_test(TestPool())
