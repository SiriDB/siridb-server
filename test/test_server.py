import random
import time
from testing import Client
from testing import default_test_setup
from testing import gen_points
from testing import gen_series
from testing import InsertError
from testing import PoolError
from testing import run_test
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError


class TestSeries(TestBase):
    title = 'Test server object'

    @default_test_setup(2)
    async def run(self):
        self.db.add_pool(servers[1], sleep=12)


if __name__ == '__main__':
    SiriDB.HOLD_TERM = False
    Server.HOLD_TERM = True
    run_test(TestSeries())
