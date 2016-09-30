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


class TestCluster(TestBase):
    title = 'Test siridb-cluster'


    @default_test_setup(4)
    async def run(self):

        await self.client0.connect()

        self.client0.close()

        # return False

if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = False
    Server.MEM_CHECK = False
    run_test(TestCluster())
