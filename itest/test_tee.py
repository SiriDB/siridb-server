import os
import asyncio
import functools
import random
import time
import math
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
from testing import SiriDB
from testing import TestBase
from testing import SiriDBAsyncUnixServer
from testing import parse_args


PIPE_NAME = '/tmp/dbtest_tee.sock'

DATA = {
    'series float': [
        [1471254705, 1.5],
        [1471254707, -3.5],
        [1471254710, -7.3]],
    'series integer': [
        [1471254705, 5],
        [1471254708, -3],
        [1471254710, -7]],
    'aggr': [
        [1447249033, 531], [1447249337, 534],
        [1447249633, 535], [1447249937, 531],
        [1447250249, 532], [1447250549, 537],
        [1447250868, 530], [1447251168, 520],
        [1447251449, 54], [1447251749, 54],
        [1447252049, 513], [1447252349, 537],
        [1447252649, 528], [1447252968, 531],
        [1447253244, 533], [1447253549, 538],
        [1447253849, 534], [1447254149, 532],
        [1447254449, 533], [1447254748, 537]],
    'huge': [
        [1471254705, 9223372036854775807],
        [1471254706, 9223372036854775806],
        [1471254707, 9223372036854775805],
        [1471254708, 9223372036854775804]],
    'equal ts': [
        [1471254705, 0], [1471254705, 1], [1471254705, 1],
        [1471254707, 0], [1471254707, 1], [1471254708, 0],
    ],
    'variance': [
        [1471254705, 2.75], [1471254706, 1.75], [1471254707, 1.25],
        [1471254708, 0.25], [1471254709, 0.5], [1471254710, 1.25],
        [1471254711, 3.5]
    ],
    'pvariance': [
        [1471254705, 0.0], [1471254706, 0.25], [1471254707, 0.25],
        [1471254708, 1.25], [1471254709, 1.5], [1471254710, 1.75],
        [1471254711, 2.75], [1471254712, 3.25]
    ],
    'filter': [
        [1471254705, 5],
        [1471254710, -3],
        [1471254715, -7],
        [1471254720, 7]
    ],
    'one': [
        [1471254710, 1]
    ],
    'log': [
        [1471254710, 'log line one'],
        [1471254712, 'log line two'],
        [1471254714, 'another line (three)'],
        [1471254716, 'and yet one more'],
    ],
    # 'special': [
    #     [1471254705, 0.1],
    #     [1471254706, math.nan],
    #     [1471254707, math.inf],
    #     [1471254708, -math.inf],
    # ]
}

if os.path.exists(PIPE_NAME):
    os.unlink(PIPE_NAME)


class TestTee(TestBase):
    title = 'Test tee'

    def on_data(self, data):
        for k, v in data.items():
            if k not in self._tee_data:
                self._tee_data[k] = []
            self._tee_data[k].extend(v)

    @default_test_setup(2)
    async def run(self):
        self._tee_data = {}

        server = SiriDBAsyncUnixServer(PIPE_NAME, self.on_data)

        await server.create()

        await self.client0.connect()

        await self.client0.query(
            'alter servers set tee_pipe_name "{}"'.format(PIPE_NAME))

        await asyncio.sleep(1)

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted 60 point(s).'})

        self.assertAlmostEqual(
            await self.client0.query('select * from "series float"'),
            {'series float': DATA['series float']})

        self.assertEqual(
            await self.client0.query('select * from "series integer"'),
            {'series integer': DATA['series integer']})

        self.assertEqual(
            await self.client0.query('select * from "log"'),
            {'log': DATA['log']})

        await asyncio.sleep(1)

        self.assertEqual(DATA, self._tee_data)

        await self.db.add_pool(self.server1, sleep=3)
        await self.assertIsRunning(self.db, self.client0, timeout=50)

        await self.client1.connect()

        self._tee_data = {}
        await self.client0.query('drop series set ignore_threshold true')

        await asyncio.sleep(1)

        await self.client0.query(
            'alter servers set tee_pipe_name "{}"'.format(PIPE_NAME))

        await asyncio.sleep(1)

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted 60 point(s).'})

        self.assertEqual(DATA, self._tee_data)

        self.client0.close()
        self.client1.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestTee())
