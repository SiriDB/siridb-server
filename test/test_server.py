import random
import time
from testing import Client
from testing import default_test_setup
from testing import gen_points
from testing import gen_series
from testing import InsertError
from testing import PoolError
from testing import QueryError
from testing import run_test
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError


class TestServer(TestBase):
    title = 'Test server object'

    @default_test_setup(2)
    async def run(self):
        await self.client0.connect()

        await self.db.add_pool(self.server1)
        await self.assertIsRunning(self.db, self.client0, timeout=12)

        await self.client1.connect()

        for port in (9010, 9011):
            result = await self.client0.query('alter server "localhost:{}" set log_level error'.format(port))
            self.assertEqual(result.pop('success_msg'), "Successful set log level to 'error' on 'localhost:{}'.".format(port))

        result = await self.client1.query('list servers log_level')
        self.assertEqual(result.pop('servers'), [['error'], ['error']])

        result = await self.client1.query('list servers uuid')

        for uuid in result.pop('servers'):
            result = await self.client0.query('alter server {} set log_level debug'.format(uuid[0]))

        result = await self.client1.query('list servers log_level')
        self.assertEqual(result.pop('servers'), [['debug'], ['debug']])

        with self.assertRaisesRegexp(
                QueryError,
                "Query error at position 42. Expecting debug, info, warning, error or critical"):
            result = await self.client0.query('alter server "localhost:{}" set log_level unknown')


        self.client0.close()
        self.client1.close()

        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = False
    run_test(TestServer())
