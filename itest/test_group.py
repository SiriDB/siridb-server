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

        with self.assertRaisesRegex(
                QueryError,
                'Group name should be at least 1 characters.'):
            await self.client0.query('create group `` for /c.*/')

        with self.assertRaisesRegex(
                QueryError,
                'Group name should be at least 1 characters.'):
            await self.client0.query('create group `` for /c.*/')

        with self.assertRaisesRegex(
                QueryError,
                'Group name should be at most [0-9]+ characters.'):
            await self.client0.query(
                'create group `{}` for /c.*/'.format('a' * 300))

        self.assertEqual(
            await self.client0.query('create group `a` for /a.*/'),
            {'success_msg': "Successfully created group 'a'."})

        with self.assertRaisesRegex(
                QueryError,
                'Group \'a\' already exists.'):
            await self.client0.query('create group `a` for /a.*/')

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

        with self.assertRaisesRegex(
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
        self.assertEqual(
            sorted(result.pop('series')),
            [['a1'], ['a2'], ['b2'], ['c2']])

        result = await self.client0.query('list series `a` ^ `two`')
        self.assertEqual(
            sorted(result.pop('series')),
            [['a1'], ['b2'], ['c2']])

        result = await self.client0.query('list series `a` - `two`')
        self.assertEqual(sorted(result.pop('series')), [['a1']])

        result = await self.client0.query('list series `a`, `two` - "c2"')
        self.assertEqual(
            sorted(result.pop('series')),
            [['a1'], ['a2'], ['b2']])

        result = await self.client0.query('list series `a`, `two` & "c2"')
        self.assertEqual(sorted(result.pop('series')), [['c2']])

        with self.assertRaisesRegex(
                QueryError,
                "Cannot compile regular expression.*"):
            await self.client1.query('alter group `a` set expression /(.*/')

        self.assertEqual(
            await self.client1.query('alter group `a` set expression /b.*/'),
            {'success_msg': "Successfully updated group 'a'."})

        # await self.client0.query('drop group `a`')
        # await self.client0.query('drop group `b`')
        await self.client0.query('drop group `c`')

        with self.assertRaisesRegex(
                QueryError,
                'Group \'c\' does not exist.'):
            await self.client0.query('drop group `c`')

        await self.client0.query('create group `all` for /.*/ # bla')

        await self.client0.query('alter group `all` set expression /.*/ # bla')

        self.assertEqual(
            await self.client0.query('count groups'),
            {'groups': 4})

        await asyncio.sleep(2)

        self.assertEqual(
            await self.client0.query('count groups where series > 2'),
            {'groups': 2})

        self.assertEqual(
            await self.client0.query('select * from * before now'),
            DATA)

        self.client0.close()
        self.client1.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestGroup())
