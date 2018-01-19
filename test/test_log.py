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


DATA = {
    # 'log': [
    #     [1471254710, 'log line one'],
    #     [1471254712, 'log line two'],
    #     [1471254714, 'log line three'],
    #     [1471254715, 'log line four'],
    #     [1471254716, 'log line five'],
    #     [1471254718, 'another line (six)'],
    #     [1471254720, 'and yet one more, this is log line seven']
    # ],
    'utf16': [
        [1471254710, ''],
        [1471254712, ''],
        [1471254714, '  '],
        [1471254715, ' '],
        [1471254716, ' '],
        [1471254718, 'ԉ']
    ]
}


class TestLog(TestBase):
    title = 'Test select and aggregate functions'

    @default_test_setup(1, compression=True, optimize_interval=10)
    async def run(self):
        await self.client0.connect()

        n = 0
        for p in DATA.values():
            n += len(p)

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted {} point(s).'.format(n)})

        # self.assertEqual(
        #     await self.client0.query('select * from "log"'),
        #     {'log': DATA['log']})

        self.assertEqual(
            await self.client0.query('select * from "utf16"'),
            {'utf16': DATA['utf16']})

        self.client0.close()

        return False

if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestLog())
