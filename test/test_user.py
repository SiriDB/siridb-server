import random
import time
from testing import Client
from testing import gen_points
from testing import InsertError
from testing import PoolError
from testing import run_test
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError


class TestUser(TestBase):
    title = 'Test user object'

    async def run(self):
        servers = [Server(i) for i in range(2)]
        for server in servers:
            server.create()
            server.start()

        time.sleep(1.0)

        db = SiriDB()
        db.create_on(servers[0])

        time.sleep(1.0)

        client0 = Client(db, servers[0])
        await client0.connect()

        result = await client0.query('list users')
        self.assertEqual(result.pop('users'), [['iris', 'full']])

        result = await client0.query('create user "sasientje" set password "blabla"')
        self.assertEqual(result.pop('success_msg'), "User 'sasientje' is created successfully.")

        result = await client0.query('list users where access < modify')
        self.assertEqual(result.pop('users'), [['sasientje', 'no access']])

        result = await client0.query('grant modify to user "sasientje"')
        self.assertEqual(result.pop('success_msg'), "Successfully granted permissions to user 'sasientje'.")

        db.add_replica(servers[1], 0)
        time.sleep(10.0)

        client1 = Client(db, servers[1])
        await client1.connect()

        result = await client1.query('list users where access < full')
        self.assertEqual(result.pop('users'), [['sasientje', 'modify']])

        result = await client1.query('create user "pee" set password "hihihaha"')
        time.sleep(0.1)
        result = await client0.query('list users where user ~ "p"')
        self.assertEqual(result.pop('users'), [['pee', 'no access']])

        client0.close()
        result = await servers[0].stop()
        self.assertTrue(result)

        result = await client1.query('alter user "sasientje" set password "dagdag"')

        servers[0].start()
        time.sleep(12.0)

        client0 = Client(db, servers[0], username="sasientje", password="dagdag")
        await client0.connect()
        result = await client0.query("show who_am_i")
        self.assertEqual(result['data'][0]['value'], 'sasientje')

        result = await client1.query('drop user "sasientje"')
        self.assertEqual(result.pop('success_msg'), "User 'sasientje' is dropped successfully.")
        time.sleep(0.1)

        result = await client0.query('count users')
        self.assertEqual(result.pop('users'), 2, msg='Expecting 2 users (iris and pee)')

        with self.assertRaisesRegexp(
                UserAuthError,
                "Access denied. User 'sasientje' has no 'grant' privileges."):
            result = await client0.query('grant full to user "pee"')

        client0.close()
        client1.close()

        for server in servers:
            result = await server.stop()
            self.assertTrue(
                result,
                msg='Server {} did not close correctly'.format(server.name))


if __name__ == '__main__':
    SiriDB.HOLD_TERM = False
    Server.HOLD_TERM = False
    run_test(TestUser())
