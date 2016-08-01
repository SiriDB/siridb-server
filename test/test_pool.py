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

    async def insert(self, client, series, n, timeout=10):
        for _ in range(n):
            while timeout:
                try:
                    await client.insert_some_series(series)
                except PoolError:
                    timeout -= 1
                else:
                    break
                finally:
                    await asyncio.sleep(1.0)


    @default_test_setup(3)
    async def run(self):

        series = gen_series(n=2000)

        await self.client0.connect()

        task0 = asyncio.ensure_future(self.insert(
            self.client0,
            series,
            30))

        await asyncio.sleep(2)

        await self.db.add_pool(self.server1, sleep=5)

        await self.client1.connect()
        task1 = asyncio.ensure_future(self.insert(
            self.client1,
            series,
            30))

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)

        await self.assertIsRunning(self.db, self.client0, timeout=90)

        await self.db.add_pool(self.server2, remote_server=self.server0, sleep=5)
        await self.client2.connect()

        task2 = asyncio.ensure_future(self.insert(
            self.client2,
            series,
            30))

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)
        await self.assertSeries(self.client2, series)

        await self.assertIsRunning(self.db, self.client0, timeout=300)

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)
        await self.assertSeries(self.client2, series)

        await asyncio.wait_for(task0, None)

        await self.assertSeries(self.client0, series)
        await self.assertSeries(self.client1, series)
        await self.assertSeries(self.client2, series)

        self.client0.close()
        self.client1.close()
        self.client2.close()

        return False

if __name__ == '__main__':
    SiriDB.HOLD_TERM = False
    Server.HOLD_TERM = False
    run_test(TestPool())
