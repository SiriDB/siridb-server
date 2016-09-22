import asyncio
import random
import time
from testing import Client
from testing import Series
from testing import default_test_setup
from testing import gen_points
from testing import gen_series
from testing import gen_data
from testing import InsertError
from testing import PoolError
from testing import run_test
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError

class TestPool(TestBase):
    title = 'Test pool object'

    async def insert(self, client, series, n, timeout=1):
        for _ in range(n):
            await client.insert_some_series(series, timeout=timeout)
            await asyncio.sleep(1.0)


    @default_test_setup(3)
    async def run(self):

        series = gen_series(n=4000)

        await self.client0.connect()

        task0 = asyncio.ensure_future(self.insert(
            self.client0,
            series,
            300))

        await asyncio.sleep(2)

        await self.db.add_pool(self.server1, sleep=3)

        with self.assertRaises(AssertionError):
            await self.db.add_pool(self.server2, sleep=3)

        await self.client1.connect()
        task1 = asyncio.ensure_future(self.insert(
            self.client1,
            series,
            200))

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)

        await self.assertIsRunning(self.db, self.client0, timeout=200)

        await self.db.add_pool(self.server2, sleep=3)
        await self.client2.connect()

        task2 = asyncio.ensure_future(self.insert(
            self.client2,
            series,
            100))

        for _ in range(10):
            await self.assertSeries(self.client0, series)
            await self.assertSeries(self.client1, series)
            await self.assertSeries(self.client2, series)
            await asyncio.sleep(2)

        await self.assertIsRunning(self.db, self.client0, timeout=600)

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)
        await self.assertSeries(self.client2, series)

        await asyncio.wait_for(task0, None)
        await asyncio.wait_for(task1, None)
        await asyncio.wait_for(task2, None)

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)
        await self.assertSeries(self.client2, series)

        self.client0.close()
        self.client1.close()
        self.client2.close()

        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = False
    run_test(TestPool())
