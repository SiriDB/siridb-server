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


class TestServer(TestBase):
    title = 'Test server object'

    Server.SERVER_ADDRESS = 'localhost'
    Server.IP_SUPPORT = 'IPV4ONLY'

    @default_test_setup(4)
    async def run(self):

        await self.client0.connect()

        await self.db.add_pool(self.server1)
        await self.assertIsRunning(self.db, self.client0, timeout=12)

        await self.client1.connect()

        for port in (9010, 9011):
            result = await self.client0.query('alter server "localhost:{}" set log_level error'.format(port))
            self.assertEqual(
                result.pop('success_msg'),
                "Successfully set log level to 'error' on 'localhost:{}'.".format(port))

        result = await self.client1.query('list servers log_level')
        self.assertEqual(result.pop('servers'), [['error'], ['error']])

        result = await self.client1.query('list servers uuid')

        for uuid in result.pop('servers'):
            result = await self.client0.query('alter server {} set log_level debug'.format(uuid[0]))

        result = await self.client1.query('list servers log_level')
        self.assertEqual(result.pop('servers'), [['debug'], ['debug']])

        result = await self.client0.query('alter servers set log_level info')
        self.assertEqual(
            result.pop('success_msg'),
            "Successfully set log level to 'info' on 2 servers.")

        result = await self.client1.query('list servers log_level')
        self.assertEqual(result.pop('servers'), [['info'], ['info']])

        result = await self.client0.query('alter servers where active_handles > 1 set log_level debug')

        result = await self.client1.query('list servers log_level')
        self.assertEqual(result.pop('servers'), [['debug'], ['debug']])

        with self.assertRaisesRegexp(
                QueryError,
                "Query error at position 42. Expecting debug, info, warning, error or critical"):
            await self.client0.query('alter server "localhost:{}" set log_level unknown')

        self.client1.close()
        result = await self.server1.stop()
        self.assertTrue(result)

        self.server1.listen_backend_port = 9111
        self.server1.create()
        await self.server1.start(sleep=35)

        await asyncio.sleep(35)

        result = await self.client0.query('list servers status')
        self.assertEqual(result.pop('servers'), [['running'], ['running']])

        await self.client1.connect()
        result = await self.client1.query('show server')
        self.assertEqual(result.pop('data'), [{'name': 'server', 'value': 'localhost:9111'}])

        await self.db.add_replica(self.server2, 1)
        await self.assertIsRunning(self.db, self.client0, timeout=10)

        with self.assertRaisesRegexp(
                QueryError,
                "Cannot remove server 'localhost:9010' because this is the only server for pool 0"):
            await self.client1.query('drop server "localhost:9010"')

        with self.assertRaisesRegexp(
                QueryError,
                "Cannot remove server 'localhost:9012' because the server is still online.*"):
            await self.client1.query('drop server "localhost:9012"')

        result = await self.server1.stop()
        self.assertTrue(result)

        result = await self.server2.stop()
        self.assertTrue(result)

        await self.server1.start(sleep=10)

        result = await self.client1.query('show status')
        self.assertEqual(result.pop('data'), [{'name': 'status', 'value': 'running | synchronizing'}])

        result = await self.client0.query('drop server "localhost:9012"')
        self.assertEqual(result.pop('success_msg'), "Successfully dropped server 'localhost:9012'.")
        self.db.servers.remove(self.server2)

        time.sleep(1)

        for client in (self.client0, self.client1):
            result = await client.query('list servers status')
            self.assertEqual(result.pop('servers'), [['running'], ['running']])

        await self.db.add_replica(self.server3, 1)
        await self.assertIsRunning(self.db, self.client0, timeout=35)

        self.client0.close()
        self.client1.close()

        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestServer())
