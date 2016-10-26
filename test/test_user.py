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


class TestUser(TestBase):
    title = 'Test user object'

    @default_test_setup(3)
    async def run(self):
        await self.client0.connect()

        result = await self.client0.query('list users')
        self.assertEqual(result.pop('users'), [['iris', 'full']])

        result = await self.client0.query('create user "sasientje" set password "blabla"')
        self.assertEqual(result.pop('success_msg'), "Successfully created user 'sasientje'.")

        result = await self.client0.query('list users where access < modify')
        self.assertEqual(result.pop('users'), [['sasientje', 'no access']])

        result = await self.client0.query('grant modify to user "sasientje"')
        self.assertEqual(result.pop('success_msg'), "Successfully granted permissions to user 'sasientje'.")

        await self.db.add_replica(self.server1, 0)
        await self.assertIsRunning(self.db, self.client0, timeout=10)
        await self.db.add_pool(self.server2)

        await self.client1.connect()
        await self.client2.connect()

        result = await self.client1.query('list users where access < full')
        self.assertEqual(result.pop('users'), [['sasientje', 'modify']])

        result = await self.client1.query('revoke write from user "sasientje"')
        self.assertEqual(result.pop('success_msg'), "Successfully revoked permissions from user 'sasientje'.")

        result = await self.client1.query('grant show, count to user "sasientje"')
        result = await self.client1.query('list users where access < modify')
        self.assertEqual(result.pop('users'), [['sasientje', 'alter, count, drop and show']])

        result = await self.client1.query('create user "pee" set password "hihihaha"')
        time.sleep(0.1)
        result = await self.client0.query('list users where name ~ "p"')
        self.assertEqual(result.pop('users'), [['pee', 'no access']])

        self.client0.close()
        result = await self.server0.stop()
        self.assertTrue(result)

        with self.assertRaisesRegexp(
                QueryError,
                "Password should be at least 4 characters."):
            result = await self.client1.query('alter user "sasientje" set password "dag"')

        result = await self.client1.query('alter user "sasientje" set password "dagdag"')

        await self.server0.start(sleep=35)

        self.client0 = Client(
            self.db,
            self.server0,
            username="sasientje",
            password="dagdag")

        await self.client0.connect()
        result = await self.client0.query("show who_am_i")
        self.assertEqual(result['data'][0]['value'], 'sasientje')

        with self.assertRaisesRegexp(
                UserAuthError,
                "Access denied. User 'sasientje' has no 'insert' privileges."):
            result = await self.client0.insert({'no access test': [[1, 1.0]]})

        result = await self.client1.query('drop user "sasientje"')
        self.assertEqual(result.pop('success_msg'), "Successfully dropped user 'sasientje'.")
        time.sleep(0.1)

        for client in (self.client0, self.client1, self.client2):
            result = await client.query('count users')
            self.assertEqual(result.pop('users'), 2, msg='Expecting 2 users (iris and pee)')

        result = await self.client0.query('count users where name == "pee"')
        self.assertEqual(result.pop('users'), 1, msg='Expecting 1 user (pee)')

        with self.assertRaisesRegexp(
                UserAuthError,
                "Access denied. User 'sasientje' has no 'grant' privileges."):
            result = await self.client0.query('grant full to user "pee"')

        with self.assertRaisesRegexp(
                QueryError,
                "User name should be at least 2 characters."):
            result = await self.client1.query('alter user "pee" set name "p"')

        with self.assertRaisesRegexp(
                QueryError,
                "^User name contains illegal characters.*"):
            result = await self.client1.query('alter user "pee" set name " p "')

        with self.assertRaisesRegexp(
                QueryError,
                "User 'iris' already exists."):
            result = await self.client1.query('alter user "pee" set name "iris"')

        with self.assertRaisesRegexp(
                QueryError,
                "User 'iris' already exists."):
            result = await self.client1.query('alter user "pee" set name "iris"')

        with self.assertRaisesRegexp(
                QueryError,
                "Cannot find user: 'Pee'"):
            result = await self.client1.query('alter user "Pee" set name "PPP"')

        result = await self.client1.query('alter user "pee" set name "Pee"')
        self.assertEqual(
            result.pop('success_msg'), "Successfully updated user 'Pee'.")

        time.sleep(0.1)
        result = await self.client2.query('list users where name == "Pee"')
        self.assertEqual(result.pop('users'), [['Pee', 'no access']])

        self.client0.close()
        self.client1.close()
        self.client2.close()

        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestUser())
