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
    'a1': [[0, 0]],
    'a2': [[0, 0]],
    'b1': [[0, 0]],
    'b2': [[0, 0]],
    'c1': [[0, 0]],
    'c2': [[0, 0]]
}


class TestGroup(TestBase):
    title = 'Test group object'

    @default_test_setup(2)
    async def run(self):
        await self.client0.connect()

        self.assertEqual(
            await self.client0.query('create group `a` for /a.*/'),
            {'success_msg': "Successfully created group 'a'."})

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted 6 point(s).'})

        time.sleep(3)

        self.assertEqual(
            await self.client0.query('count series `a`'),
            {'series': 2})

        self.assertEqual(
            await self.client0.query('create group `b` for /b.*/'),
            {'success_msg': "Successfully created group 'b'."})

        time.sleep(3)

        self.assertEqual(
            await self.client0.query('count series `b`'),
            {'series': 2})

        await self.db.add_pool(self.server1)
        await self.assertIsRunning(self.db, self.client0, timeout=60)
        await self.client1.connect()

        self.assertEqual(
            await self.client1.query('create group `c` for /c.*/'),
            {'success_msg': "Successfully created group 'c'."})

        time.sleep(3)

        result = await self.client1.query('list groups series')
        self.assertEqual(result.pop('groups'), [[2], [2], [2]])

        with self.assertRaisesRegexp(
                QueryError,
                "Cannot compile regular expression.*"):
            result = await self.client1.query('create group `invalid` for /(/')

        self.assertEqual(
            await self.client1.query('create group `one` for /.1/'),
            {'success_msg': "Successfully created group 'one'."})

        self.assertEqual(
            await self.client1.query('alter group `one` set name "two"'),
            {'success_msg': "Successfully updated group 'two'."})

        self.assertEqual(
            await self.client1.query('alter group `two` set expression /.2/'),
            {'success_msg': "Successfully updated group 'two'."})

        time.sleep(3)

        result = await self.client0.query('list series `a` & `two`')
        self.assertEqual(sorted(result.pop('series')), [['a2']])

        result = await self.client0.query('list series `a` | `two`')
        self.assertEqual(sorted(result.pop('series')), [['a1'], ['a2'], ['b2'], ['c2']])

        result = await self.client0.query('list series `a` ^ `two`')
        self.assertEqual(sorted(result.pop('series')), [['a1'], ['b2'], ['c2']])

        result = await self.client0.query('list series `a` - `two`')
        self.assertEqual(sorted(result.pop('series')), [['a1']])

        result = await self.client0.query('list series `a`, `two` - "c2"')
        self.assertEqual(sorted(result.pop('series')), [['a1'], ['a2'], ['b2']])

        result = await self.client0.query('list series `a`, `two` & "c2"')
        self.assertEqual(sorted(result.pop('series')), [['c2']])


        self.client0.close()
        self.client1.close()

        return False

if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'INFO'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestGroup())
