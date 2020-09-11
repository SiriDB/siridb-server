import asyncio
import functools
import random
import time
import math
import re
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


class TestAutoDuration(TestBase):
    title = 'Test select and aggregate functions'

    @default_test_setup(2, compression=False, buffer_size=1024)
    async def run(self):
        await self.client0.connect()

        gap =
        ts = int(time.time()) -

        points = []

        for i in range(tx,)
            [ts, i]
            for i
        ]

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted {} point(s).'.format(
                LENPOINTS)})

        self.assertEqual(
            await self.client0.query(
                'select difference() from "series-001 integer"'),
            {'series-001 integer': [[1471254708, -8], [1471254710, -4]]})

        self.assertEqual(
            await self.client0.query(
                'select difference() => difference() '
                'from "series-001 integer"'),
            {'series-001 integer': [[1471254710, 4]]})

        self.assertEqual(
            await self.client0.query(
                'select difference() => difference() => difference() '
                'from "series-001 integer"'),
            {'series-001 integer': []})

        now = int(time.time())
        self.assertEqual(
            await self.client0.query(
                'select difference({}) from "series-001 integer"'.format(now)),
            {'series-001 integer': [[now, -12]]})

        now = int(time.time())
        self.assertEqual(
            await self.client0.query(
                'select difference({}) from "series-001 integer"'.format(now)),
            {'series-001 integer': [[now, -12]]})

        self.assertEqual(
            await self.client0.query(
                'select * from /series-001.*/ '
                'merge as "median_low" using median_low({})'
                .format(now)),
            {'median_low': [[now, -3.5]]})

        self.assertEqual(
            await self.client0.query(
                'select * from /series-001.*/ '
                'merge as "median_high" using median_high({})'
                .format(now)),
            {'median_high': [[now, -3.0]]})

        self.assertEqual(
            await self.client0.query(
                'select * from /series.*/ '
                'merge as "max" using max(1s)'),
            {'max': [
                [1471254705, 5.0],
                [1471254707, -2.5],
                [1471254708, -1.0],
                [1471254710, -7.0]
            ]})

        # Test all aggregation methods

        self.assertEqual(
            await self.client0.query('select sum(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 2663], [1447254000, 5409], [1447257600, 1602]]})

        self.assertEqual(
            await self.client0.query('select count(1h) from "aggr"'),
            {'aggr': [[1447250400, 5], [1447254000, 12], [1447257600, 3]]})

        self.assertEqual(
            await self.client0.query('select mean(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 532.6],
                [1447254000, 450.75],
                [1447257600, 534.0]]})

        self.assertEqual(
            await self.client0.query('select median(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 532.0],
                [1447254000, 530.5],
                [1447257600, 533.0]]})

        self.assertEqual(
            await self.client0.query('select median_low(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 532], [1447254000, 530], [1447257600, 533]]})

        self.assertEqual(
            await self.client0.query('select median_high(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 532], [1447254000, 531], [1447257600, 533]]})

        self.assertEqual(
            await self.client0.query('select min(1h) from "aggr"'),
            {'aggr': [[1447250400, 531], [1447254000, 54], [1447257600, 532]]})

        self.assertEqual(
            await self.client0.query('select max(1h) from "aggr"'),
            {'aggr': [
                [1447250400, 535], [1447254000, 538], [1447257600, 537]]})

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
            await self.client0.query('select * from ({}) - ("a", "b")'.format(
                ','.join(['"aggr"'] * 600)
            )),
            {'aggr': DATA['aggr']}
        )

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

        self.assertEqual(
            await self.client0.query(
                'select filter(/l.*/) from * where type == string'),
            {'log': [p for p in DATA['log'] if re.match('l.*', p[1])]})

        self.assertEqual(
            await self.client0.query(
                'select filter(==/l.*/) from * where type == string'),
            {'log': [p for p in DATA['log'] if re.match('l.*', p[1])]})

        self.assertEqual(
            await self.client0.query(
                'select filter(!=/l.*/) from * where type == string'),
            {'log': [p for p in DATA['log'] if not re.match('l.*', p[1])]})

        self.assertEqual(
            await self.client0.query('select limit(300, mean) from "aggr"'),
            {'aggr': DATA['aggr']})

        self.assertEqual(
            await self.client0.query('select limit(1, sum)  from "aggr"'),
            {'aggr': [[1447254748, 9674]]})

        self.assertEqual(
            await self.client0.query('select limit(3, mean) from "aggr"'),
            {'aggr': [
                [1447250938, 532.8571428571429],
                [1447252844, 367.6666666666667],
                [1447254750, 534.0]]})

        self.assertEqual(
            await self.client0.query(
                'select limit(2, max)  from "series-001 float"'),
            {'series-001 float': [[1471254707, 1.5], [1471254713, -7.3]]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select variance(1471254712) from "variance"'),
            {'variance': [[1471254712, 1.3720238095238095]]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select pvariance(1471254715) from "pvariance"'),
            {'pvariance': [[1471254715, 1.25]]})

        self.assertEqual(
            await self.client0.query('select * from "one"'),
            {'one': [[1471254710, 1]]})

        self.assertEqual(
            await self.client0.query('select * from "log"'),
            {'log': DATA['log']})

        self.assertEqual(
            await self.client0.query(
                'select filter(~"log") => filter(!~"one") from "log"'),
            {'log': [DATA['log'][1]]})

        self.assertEqual(
            await self.client0.query(
                'select filter(!=nan) from "special"'),
            {'special': [p for p in DATA['special'] if not math.isnan(p[1])]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(==nan) from "special"'),
            {'special': [p for p in DATA['special'] if math.isnan(p[1])]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(>=nan) from "special"'),
            {'special': [p for p in DATA['special'] if math.isnan(p[1])]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(<=nan) from "special"'),
            {'special': [p for p in DATA['special'] if math.isnan(p[1])]})

        self.assertEqual(
            await self.client0.query(
                'select filter(==inf) from "special"'),
            {'special': [p for p in DATA['special'] if p[1] == math.inf]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(<inf) from "special"'),
            {'special': [p for p in DATA['special'] if p[1] < math.inf]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(>inf) from "special"'),
            {'special': []})

        self.assertEqual(
            await self.client0.query(
                'select filter(==-inf) from "special"'),
            {'special': [p for p in DATA['special'] if p[1] == -math.inf]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(>-inf) from "special"'),
            {'special': [p for p in DATA['special'] if p[1] > -math.inf]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select filter(<-inf) from "special"'),
            {'special': []})

        self.assertEqual(
            await self.client0.query(
                'select filter(~"one") prefix "1-", '
                'filter(~"two") prefix "2-" from "log"'),
            {
                '1-log': [
                    [1471254710, 'log line one'],
                    [1471254716, 'and yet one more']],
                '2-log': [[1471254712, 'log line two']]
            })

        self.assertEqual(
            await self.client0.query('select difference() from "one"'),
            {'one': []})

        with self.assertRaisesRegex(
                QueryError,
                'Regular expressions can only be used with.*'):
            await self.client0.query('select filter(~//) from "log"')

        with self.assertRaisesRegex(
                QueryError,
                'Cannot use a string filter on number type.'):
            await self.client0.query('select filter(//) from "aggr"')

        with self.assertRaisesRegex(
                QueryError,
                r'Cannot use mean\(\) on string type\.'):
            await self.client0.query('select mean(1w) from "log"')

        with self.assertRaisesRegex(
                QueryError,
                r'Group by time must be an integer value larger than zero\.'):
            await self.client0.query('select mean(0) from "aggr"')

        with self.assertRaisesRegex(
                QueryError,
                r'Limit must be an integer value larger than zero\.'):
            await self.client0.query('select limit(6 - 6, mean) from "aggr"')

        with self.assertRaisesRegex(
                QueryError,
                r'Cannot use a string filter on number type\.'):
            await self.client0.query(
                'select * from "aggr" '
                'merge as "t" using filter("0")')

        with self.assertRaisesRegex(
                QueryError,
                r'Cannot use difference\(\) on string type\.'):
            await self.client0.query('select difference() from "log"')

        with self.assertRaisesRegex(
                QueryError,
                r'Cannot use derivative\(\) on string type\.'):
            await self.client0.query('select derivative(6, 3) from "log"')

        with self.assertRaisesRegex(
                QueryError,
                r'Cannot use derivative\(\) on string type\.'):
            await self.client0.query('select derivative() from "log"')

        with self.assertRaisesRegex(
                QueryError,
                r'Overflow detected while using sum\(\)\.'):
            await self.client0.query('select sum(now) from "huge"')

        with self.assertRaisesRegex(
                QueryError,
                'Max depth reached in \'where\' expression!'):
            await self.client0.query(
                'select * from "aggr" where ((((((length > 1))))))')

        with self.assertRaisesRegex(
                QueryError,
                'Cannot compile regular expression.*'):
            await self.client0.query(
                'select * from /(bla/')

        with self.assertRaisesRegex(
                QueryError,
                'Memory allocation error or maximum recursion depth reached.'):
            await self.client0.query(
                'select * from {}"aggr"{}'.format(
                    '(' * 501,
                    ')' * 501))

        with self.assertRaisesRegex(
                    QueryError,
                    'Query too long.'):
            await self.client0.query('select * from "{}"'.format('a' * 65535))

        with self.assertRaisesRegex(
                    QueryError,
                    'Error while merging points. Make sure the destination '
                    'series name is valid.'):
            await self.client0.query(
                'select * from "aggr", "huge" merge as ""')

        self.assertEqual(
            await self.client0.query(
                'select min(2h) prefix "min-", max(1h) prefix "max-" '
                'from /.*/ where type == integer and name != "filter" '
                'and name != "one" and name != "series-002 integer" '
                'merge as "int_min_max" using median_low(1) => difference()'),
            {
                'max-int_min_max': [
                    [1447254000, 3], [1447257600, -1], [1471255200, -532]],
                'min-int_min_max': [
                    [1447257600, -477], [1471255200, -54]]})

        await self.client0.query('select derivative() from "equal ts"')

        self.assertEqual(
            await self.client0.query('select first() from *'),
            {k: [v[0]] for k, v in DATA.items()})

        self.assertEqual(
            await self.client0.query('select last() from *'),
            {k: [v[-1]] for k, v in DATA.items()})

        self.assertEqual(
            await self.client0.query('select count() from *'),
            {k: [[v[-1][0], len(v)]] for k, v in DATA.items()})

        self.assertEqual(
            await self.client0.query('select mean() from "aggr"'),
            {'aggr': [[
                DATA['aggr'][-1][0],
                sum([x[1] for x in DATA['aggr']]) / len(DATA['aggr'])]]})

        self.assertAlmostEqual(
            await self.client0.query('select stddev() from "aggr"'),
            {'aggr': [[
                DATA['aggr'][-1][0],
                147.07108914792838]]})

        self.assertAlmostEqual(
            await self.client0.query('select stddev(1h) from "aggr"'),
            {"aggr": [
                [1447250400, 1.8165902124584952],
                [1447254000, 185.46409846162092],
                [1447257600, 2.6457513110645907]]})

        # test prefix, suffex
        result = await self.client0.query(
                'select sum(1d) prefix "sum-" suffix "-sum", '
                'min(1d) prefix "minimum-", '
                'max(1d) suffix "-maximum" from "aggr"')

        self.assertIn('sum-aggr-sum', result)
        self.assertIn('minimum-aggr', result)
        self.assertIn('aggr-maximum', result)

        await self.client0.query('alter database set select_points_limit 10')
        with self.assertRaisesRegex(
                QueryError,
                'Query has reached the maximum number of selected points.*'):
            await self.client0.query(
                'select * from /.*/')
        await self.client0.query(
            'alter database set select_points_limit 1000000')

        self.client0.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestSelect())
