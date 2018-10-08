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


PI = 'ԉ'
Klingon = '     ' + \
    'qajunpaQHeylIjmo’ batlh DuSuvqang charghwI’ ‘It.'

data = {
    'string': [
        [1538660000, "some string value"],
        [1538660010, -123456789],
        [1538660020, -0.5],
        [1538660030, 1/3],
    ],
    'integer': [
        [1538660000, 1],
        [1538660010, 35.6],
        [1538660020, "-50%"],
        [1538660030, ""],
        [1538660035, "garbage"],
        [1538660040, "18446744073709551616"],
        [1538660050, "-18446744073709551616"],
    ],
    'double': [
        [1538660000, 1.0],
        [1538660010, -35],
        [1538660010, "-50%"],
        [1538660030, ""],
        [1538660035, "garbage"],
    ]
}

expected = {
    'string': [
        [1538660000, "some string value"],
        [1538660010, '-123456789'],
        [1538660020, '-0,500000'],
        [1538660030, '0,333333'],
    ],
    'integer': [
        [1538660000, 1],
        [1538660010, 35],
        [1538660020, -50],
        [1538660030, 0],
        [1538660035, 0],
        [1538660040, 9223372036854775807],
        [1538660050, -9223372036854775808],
    ],
    'double': [
        [1538660000, 1.0],
        [1538660010, -35.0],
        [1538660010, -50.0],
        [1538660030, 0.0],
        [1538660035, 0.0],
    ]
}


class TestSeries(TestBase):
    title = 'Test series object'

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        points = gen_points(n=10)

        self.assertEqual(
            await self.client0.insert({
                PI: points,
                Klingon: points
            }), {'success_msg': 'Successfully inserted 20 point(s).'})

        self.assertEqual(
            await self.client0.query('select * from "{}"'.format(PI)),
            {PI: sorted(points)})

        self.assertEqual(
            await self.client0.query('select * from "{}"'.format(Klingon)),
            {Klingon: sorted(points)})

        self.assertEqual(
            await self.client0.insert(data),
            {'success_msg': 'Successfully inserted 16 point(s).'})

        self.assertAlmostEqual(
            await self.client0.query(
                'select * from "string", "integer", "double"'),
            expected)

        self.client0.close()

        # return False


if __name__ == '__main__':
    parse_args()
    run_test(TestSeries())
