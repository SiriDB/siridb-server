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
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError
from testing import parse_args


DATA = {
    'series-001 float': [
        [1471254705, 1.5],
        [1471254707, -3.5],
        [1471254710, -7.3]],
    'series-001 integer': [
        [1471254705, 5],
        [1471254708, -3],
        [1471254710, -7]],
    'series-002 float': [
        [1471254705, 3.5],
        [1471254707, -2.5],
        [1471254710, -8.3]],
    'series-002 integer': [
        [1471254705, 4],
        [1471254708, -1],
        [1471254710, -8]],
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
    'special': [
        [1471254705, 0.1],
        [1471254706, math.nan],
        [1471254707, math.inf],
        [1471254708, -math.inf],
    ]
}


class TestTags(TestBase):
    title = 'Test tag and untag series'

    @default_test_setup(3, time_precision='s')
    async def run(self):
        await self.client0.connect()

        await self.client0.insert(DATA)

        res = await self.client0.query('''
            alter series /series.*/ tag `SERIES`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 4 series."})

        res = await self.client0.query('''
            alter series /.*/ tag `ALL`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 13 series."})

        res = await self.client0.query('''
            alter series /empty/ tag `EMPTY`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 0 series."})

        await asyncio.sleep(3.0)

        res = await self.client0.query('''
            alter series `ALL` - `SERIES` tag `OTHER`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 9 series."})

        await self.db.add_pool(self.server1)

        await asyncio.sleep(3.0)

        res = await self.client0.query('''
            alter series /series-00(1|2) integer/ tag `SERIES_INT`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 2 series."})

        res = await self.client0.query('''
            alter series 'one' untag `OTHER`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully untagged 1 series."})

        await self.assertIsRunning(self.db, self.client0, timeout=45)

        await asyncio.sleep(5)

        await self.db.add_replica(self.server2, 0)
        await asyncio.sleep(3.0)

        res = await self.client0.query('''
            alter series /series-00(1|2) float/ tag `SERIES_FLOAT`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 2 series."})

        res = await self.client0.query('''
            alter series 'huge' untag `OTHER`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully untagged 1 series."})

        await self.assertIsRunning(self.db, self.client0, timeout=45)

        res = await self.client0.query('''
            alter series 'one', 'huge', 'log' tag `SPECIAL`
        ''')
        self.assertEqual(
            res, {"success_msg": "Successfully tagged 3 series."})

        await self.client0.query('''
            alter series 'variance', 'pvariance' untag `OTHER`
        ''')

        await self.client0.query('''
            alter series `ALL` where type == float tag `F`
        ''')

        await self.client0.query('''
            alter series `ALL` tag `I`
        ''')

        await asyncio.sleep(3.0)

        await self.client0.query('''
            alter series `ALL` where type != integer untag `I`
        ''')

        await self.client1.connect()
        await self.client2.connect()

        for client in (self.client0, self.client1, self.client2):
            res = await self.client0.query('''
                list tags name, series
            ''')
            tags = sorted(res['tags'])
            self.assertEqual(tags, [
                ["ALL", 13],
                ["EMPTY", 0],
                ["F", 5],
                ["I", 7],
                ["OTHER", 5],
                ["SERIES", 4],
                ["SERIES_FLOAT", 2],
                ["SERIES_INT", 2],
                ["SPECIAL", 3],
            ])

        for series in ('huge', 'log', 'series-001 integer', 'one'):
            await self.client0.query('''
                drop series '{0}'
            '''.format(series))

        await asyncio.sleep(6.0)  # groups and tags are updates each 3 seconds

        for client in (self.client0, self.client1, self.client2):
            res = await self.client0.query('''
                list tags name, series
            ''')
            tags = sorted(res['tags'])
            self.assertEqual(tags, [
                ["ALL", 9],
                ["EMPTY", 0],
                ["F", 5],
                ["I", 4],
                ["OTHER", 4],
                ["SERIES", 3],
                ["SERIES_FLOAT", 2],
                ["SERIES_INT", 1],
                ["SPECIAL", 0],
            ])

        for tag in (
                'ALL',
                'EMPTY',
                'OTHER',
                'SERIES',
                'SERIES_FLOAT',
                'SERIES_INT',
                'SPECIAL'):
            await self.client0.query('''
                drop tag `{0}`
            '''.format(tag))

        await asyncio.sleep(1.5)

        for client in (self.client0, self.client1, self.client2):
            res = await client.query('''
                list tags name, series
            ''')
            tags = sorted(res['tags'])
            self.assertEqual(tags, [
                ["F", 5],
                ["I", 4],
            ])

        await self.client0.query('''
            alter tag `F` set name 'Float'
        ''')

        await asyncio.sleep(1.5)

        for client in (self.client0, self.client1, self.client2):
            res = await client.query('''
                list tags name, series
            ''')
            tags = sorted(res['tags'])
            self.assertEqual(tags, [
                ["Float", 5],
                ["I", 4],
            ])

        self.client2.close()
        self.client1.close()
        self.client0.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestTags())
