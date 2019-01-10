import asyncio
import functools
import os
import subprocess
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


LENPOINTS = 12
DATA = {
    'series-001': [
        [1471254705000000005, 1.5],
        [1471254705000000007, -3.5],
        [1471254705000000010, -7.3]],
    'series-002': [
        [1471254705000000005, 5],
        [1471254705000000008, -3],
        [1471254705000000010, -7]],
    'series-003': [
        [1471254705000000005, 10.5],
        [1471254705000000007, -8.5],
        [1471254705000000010, -2.7]],
    'series-004': [
        [1471254705000000005, 6],
        [1471254705000000008, -8],
        [1471254705000000010, -9]],
}

TIME_PRECISION = 'ns'


class TestCreateDatabase(TestBase):
    title = 'Test create database'

    @default_test_setup(2, time_precision=TIME_PRECISION)
    async def run(self):
        await self.client0.connect()

        tasks = [
            asyncio.ensure_future(
                SiriDB(
                    dbname="db_{}".format(i),
                    time_precision=TIME_PRECISION).create_on(
                        server=self.server0
                        ))
            for i in range(30)]

        await asyncio.gather(*tasks)

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted {} point(s).'.format(
                LENPOINTS)})

        self.assertEqual(
            await self.client0.query('create group `b` for /series.*/'),
            {'success_msg': "Successfully created group 'b'."})

        time.sleep(3)

        self.client0.close()

        for i in range(30):
            client = Client(
                db=SiriDB(dbname="db_{}".format(i)),
                servers=self.servers)
            await client.connect()
            self.assertEqual(
                await client.insert(DATA),
                {'success_msg': 'Successfully inserted {} point(s).'.format(
                    LENPOINTS)})
            client.close()

        await self.client0.connect()

        self.assertEqual(
            await self.client0.query(
                'select max(1) from /series.*/ '
                'merge as "max" using max(1)'),
            {'max': [
                [1471254705000000005, 10.5],
                [1471254705000000007, -3.5],
                [1471254705000000008, -3.0],
                [1471254705000000010, -2.7]
            ]})

        await self.db.add_pool(self.server1)
        await self.assertIsRunning(self.db, self.client0, timeout=20)
        await asyncio.sleep(45)

        await SiriDB(dbname="dbtest").drop_db(server=self.server1)

        tasks = [
            asyncio.ensure_future(
                SiriDB(
                    dbname="db_{}".format(i)).drop_db(
                        server=self.server0
                        ))
            for i in range(10)]

        await asyncio.gather(*tasks)

        self.client0.close()

if __name__ == '__main__':
    parse_args()
    run_test(TestCreateDatabase())
