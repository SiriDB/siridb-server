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


class TestCluster(TestBase):
    title = 'Test siridb-cluster'

    @default_test_setup(4, time_precision='s')
    async def run(self):
        await self.client0.connect()

        await self.db.add_pool(self.server1)
        await self.assertIsRunning(self.db, self.client0, timeout=12)

        await asyncio.sleep(45)

        await self.db.add_replica(self.server2, 0)
        await self.assertIsRunning(self.db, self.client0, timeout=12)

        await asyncio.sleep(45)

        await self.db.add_replica(self.server3, 1)
        await self.assertIsRunning(self.db, self.client0, timeout=12)

        # await asyncio.sleep(35)

        # await self.db.add_pool(self.server4)
        # await self.assertIsRunning(self.db, self.client0, timeout=12)

        # await asyncio.sleep(35)

        # await self.db.add_pool(self.server5)
        # await self.assertIsRunning(self.db, self.client0, timeout=12)

        # await self.db.add_replica(self.server1, 0)
        # await asyncio.sleep(5)

        # await self.db.add_replica(self.server3, 1)
        # await asyncio.sleep(5)

        # await self.db.add_replica(self.server5, 2)

        # await self.assertIsRunning(self.db, self.client0, timeout=35)

        # self.client0.close()

        return False


if __name__ == '__main__':
    parse_args()
    run_test(TestCluster())
