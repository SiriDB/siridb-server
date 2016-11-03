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
    'series float': [
        [1471254705, 1.5],
        [1471254707, -3.5],
        [1471254710, -7.3]],
    'series integer': [
        [1471254705, 5],
        [1471254708, -3],
        [1471254710, -7]],
    'aggr': [
        [1447249033, 531], [1447249337, 534], [1447249633, 535], [1447249937, 531],
        [1447250249, 532], [1447250549, 537], [1447250868, 530], [1447251168, 520],
        [1447251449, 54], [1447251749, 54], [1447252049, 513], [1447252349, 537],
        [1447252649, 528], [1447252968, 531], [1447253244, 533], [1447253549, 538],
        [1447253849, 534], [1447254149, 532], [1447254449, 533], [1447254748, 537]],
    'huge': [
        [1471254705, 9223372036854775807],
        [1471254706, 9223372036854775806],
        [1471254707, 9223372036854775805],
        [1471254708, 9223372036854775804]],
    'equal ts': [
        [1471254705, 0], [1471254705, 1], [1471254705, 1],
        [1471254707, 0], [1471254707, 1], [1471254708, 0],
    ]
}


class TestSelect(TestBase):
    title = 'Test select and aggregate functions'

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted 36 point(s).'})

        self.assertEqual(
            await self.client0.query(
                'select difference() from "series integer"'),
            {'series integer': [[1471254708, -8], [1471254710, -4]]})

        self.assertEqual(
            await self.client0.query(
                'select difference() => difference() from "series integer"'),
            {'series integer': [[1471254710, 4]]})

        self.assertEqual(
            await self.client0.query(
                'select difference() => difference() => difference() from "series integer"'),
            {'series integer': []})

        now = int(time.time())
        self.assertEqual(
            await self.client0.query(
                'select difference({}) from "series integer"'.format(now)),
            {'series integer': [[now, -12]]})

        now = int(time.time())
        self.assertEqual(
            await self.client0.query(
                'select difference({}) from "series integer"'.format(now)),
            {'series integer': [[now, -12]]})

        self.assertEqual(
            await self.client0.query(
                'select * from /series.*/ merge as "median_low" using median_low({})'
                .format(now)),
            {'median_low': [[now, -3.5]]})

        self.assertEqual(
            await self.client0.query(
                'select * from /series.*/ merge as "median_high" using median_high({})'
                .format(now)),
            {'median_high': [[now, -3.0]]})

        # Test all aggregation methods

        self.assertEqual(
            await self.client0.query('select sum(1h) from "aggr"'),
            {'aggr': [[1447250400, 2663], [1447254000, 5409], [1447257600, 1602]]})


        self.assertEqual(
            await self.client0.query('select count(1h) from "aggr"'),
            {'aggr': [[1447250400, 5], [1447254000, 12], [1447257600, 3]]})


        self.assertEqual(
            await self.client0.query('select mean(1h) from "aggr"'),
            {'aggr': [[1447250400, 532.6], [1447254000, 450.75], [1447257600, 534.0]]})

        self.assertEqual(
            await self.client0.query('select median(1h) from "aggr"'),
            {'aggr': [[1447250400, 532.0], [1447254000, 530.5], [1447257600, 533.0]]})

        self.assertEqual(
            await self.client0.query('select median_low(1h) from "aggr"'),
            {'aggr': [[1447250400, 532], [1447254000, 530], [1447257600, 533]]})

        self.assertEqual(
            await self.client0.query('select median_high(1h) from "aggr"'),
            {'aggr': [[1447250400, 532], [1447254000, 531], [1447257600, 533]]})

        self.assertEqual(
            await self.client0.query('select min(1h) from "aggr"'),
            {'aggr': [[1447250400, 531], [1447254000, 54], [1447257600, 532]]})

        self.assertEqual(
            await self.client0.query('select max(1h) from "aggr"'),
            {'aggr': [[1447250400, 535], [1447254000, 538], [1447257600, 537]]})

        self.assertAlmostEqual(
            await self.client0.query('select variance(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 3.3],
                [1447254000, 34396.931818181816],
                [1447257600, 7.0]]})

        self.assertAlmostEqual(
            await self.client0.query('select pvariance(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 2.6399999999999997],
                [1447254000, 31530.520833333332],
                [1447257600, 4.666666666666667]]})

        self.assertEqual(
            await self.client0.query('select difference(1h) from "aggr"'),
            {'aggr': [[1447250400, 1], [1447254000, -3], [1447257600, 5]]})

        self.assertAlmostEqual(
            await self.client0.query('select derivative(1, 1h) from "aggr"'),
            {'aggr': [
                [1447250400, 0.0002777777777777778],
                [1447254000, -0.0008333333333333333],
                [1447257600, 0.001388888888888889]]})

        self.assertEqual(
            await self.client0.query('select filter(>534) from "aggr"'),
            {'aggr': [
                [1447249633, 535],
                [1447250549, 537],
                [1447252349, 537],
                [1447253549, 538],
                [1447254748, 537]]})

        with self.assertRaisesRegexp(
                QueryError,
                'Cannot use a string filter on number type\.'):
            await self.client0.query(
                'select * from "aggr" '
                'merge as "t" using filter("0")')

        with self.assertRaisesRegexp(
                QueryError,
                'Overflow detected while using sum\(\)\.'):
            await self.client0.query('select sum(now) from "huge"')

        with self.assertRaisesRegexp(
                QueryError,
                'Max depth reached in \'where\' expression!'):
            await self.client0.query(
                'select * from "aggr" where ((((((length > 1))))))')

        with self.assertRaisesRegexp(
                QueryError,
                'Cannot compile regular expression.*'):
            await self.client0.query(
                'select * from /(bla/')

        with self.assertRaisesRegexp(
                QueryError,
                'Memory allocation error or maximum recursion depth reached.'):
            await self.client0.query(
                'select * from "aggr" where length > {}'.format('('*500))

        with self.assertRaisesRegexp(
                    QueryError,
                    'Query too long.'):
            await self.client0.query('select * from "{}"'.format('a' * 65535))


        await self.client0.query('select derivative() from "equal ts"')

        # test prefix, suffex
        result = await self.client0.query(
                'select sum(1d) prefix "sum-" suffix "-sum", '
                'min(1d) prefix "minimum-", '
                'max(1d) suffix "-maximum" from "aggr"');

        self.assertIn('sum-aggr-sum', result)
        self.assertIn('minimum-aggr', result)
        self.assertIn('aggr-maximum', result)


        self.client0.close()

        return False

if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestSelect())
