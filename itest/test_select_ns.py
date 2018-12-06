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


LENPOINTS = 70
DATA = {
    'series-001 float': [
        [1471254705000000005, 1.5],
        [1471254705000000007, -3.5],
        [1471254705000000010, -7.3]],
    'series-001 integer': [
        [1471254705000000005, 5],
        [1471254705000000008, -3],
        [1471254705000000010, -7]],
    'series-002 float': [
        [1471254705000000005, 3.5],
        [1471254705000000007, -2.5],
        [1471254705000000010, -8.3]],
    'series-002 integer': [
        [1471254705000000005, 4],
        [1471254705000000008, -1],
        [1471254705000000010, -8]],
    'aggr': [
        [1447249049033000000, 531], [1447249049337000000, 534],
        [1447249049633000000, 535], [1447249049937000000, 531],
        [1447249050249000000, 532], [1447249050549000000, 537],
        [1447249050868000000, 530], [1447249051168000000, 520],
        [1447249051449000000, 54], [1447249051749000000, 54],
        [1447249052049000000, 513], [1447249052349000000, 537],
        [1447249052649000000, 528], [1447249052968000000, 531],
        [1447249053244000000, 533], [1447249053549000000, 538],
        [1447249053849000000, 534], [1447249054149000000, 532],
        [1447249054449000000, 533], [1447249054748000000, 537]],

    'huge': [
        [1471254705000000005, 9223372036854775807],
        [1471254705000000006, 9223372036854775806],
        [1471254705000000007, 9223372036854775805],
        [1471254705000000008, 9223372036854775804]],
    'equal ts': [
        [1471254705000000005, 0], [1471254705000000005, 1],
        [1471254705000000005, 1], [1471254705000000007, 0],
        [1471254705000000007, 1], [1471254705000000007, 0],
    ],
    'variance': [
        [1471254705000000005, 2.75], [1471254705000000006, 1.75],
        [1471254705000000007, 1.25], [1471254705000000008, 0.25],
        [1471254705000000009, 0.5], [1471254705000000010, 1.25],
        [1471254705000000011, 3.5]
    ],
    'pvariance': [
        [1471254705000000005, 0.0], [1471254705000000006, 0.25],
        [1471254705000000007, 0.25], [1471254705000000008, 1.25],
        [1471254705000000009, 1.5], [1471254705000000010, 1.75],
        [1471254705000000011, 2.75], [1471254705000000012, 3.25]
    ],
    'filter': [
        [1471254705000000005, 5],
        [1471254705000000010, -3],
        [1471254705000000015, -7],
        [1471254705000000020, 7]
    ],
    'one': [
        [1471254705000000010, 1]
    ],
    'log': [
        [1471254705000000010, 'log line one'],
        [1471254705000000012, 'log line two'],
        [1471254705000000014, 'another line (three)'],
        [1471254705000000016, 'and yet one more'],
    ],
    'special': [
        [1471254705000000005, 0.1],
        [1471254705000000006, math.nan],
        [1471254705000000007, math.inf],
        [1471254705000000008, -math.inf],
    ]
}


TIME_PRECISION = 'ns'


class TestSelectNano(TestBase):
    title = 'Test select and aggregate functions'

    GEN_POINTS = functools.partial(
        gen_points, n=1, time_precision=TIME_PRECISION)

    @default_test_setup(1, time_precision=TIME_PRECISION)
    async def run(self):
        await self.client0.connect()

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted {} point(s).'.format(
                LENPOINTS)})

        self.assertEqual(
            await self.client0.query(
                'select difference() from "series-001 integer"'),
            {'series-001 integer': [
                [1471254705000000008, -8],
                [1471254705000000010, -4]
            ]})

        self.assertEqual(
            await self.client0.query(
                'select difference() => difference() '
                'from "series-001 integer"'),
            {'series-001 integer': [[1471254705000000010, 4]]})

        self.assertEqual(
            await self.client0.query(
                'select difference() => difference() => difference() '
                'from "series-001 integer"'),
            {'series-001 integer': []})

        now = int(time.time()*1000000000)
        self.assertEqual(
            await self.client0.query(
                'select difference({}) from "series-001 integer"'.format(now)),
            {'series-001 integer': [[now, -12]]})

        now = int(time.time()*1000000000)
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
                'select max(1) from /series.*/ '
                'merge as "max" using max(1)'),
            {'max': [
                [1471254705000000005, 5.0],
                [1471254705000000007, -2.5],
                [1471254705000000008, -1.0],
                [1471254705000000010, -7.0]
            ]})

        # Test all aggregation methods

        self.assertEqual(
            await self.client0.query('select sum(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 2131],
                [1447249051000000000, 1599],
                [1447249052000000000, 628],
                [1447249053000000000, 2109],
                [1447249054000000000, 1605],
                [1447249055000000000, 1602]
            ]})

        self.assertEqual(
            await self.client0.query('select count(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 4],
                [1447249051000000000, 3],
                [1447249052000000000, 3],
                [1447249053000000000, 4],
                [1447249054000000000, 3],
                [1447249055000000000, 3]
            ]})

        self.assertEqual(
            await self.client0.query('select mean(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 532.75],
                [1447249051000000000, 533.0],
                [1447249052000000000, 209.3333333333333333334],
                [1447249053000000000, 527.25],
                [1447249054000000000, 535.0],
                [1447249055000000000, 534.0]
            ]})

        self.assertEqual(
            await self.client0.query('select median(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 532.5],
                [1447249051000000000, 532.0],
                [1447249052000000000, 54.0],
                [1447249053000000000, 529.5],
                [1447249054000000000, 534.0],
                [1447249055000000000, 533.0]
            ]})

        self.assertEqual(
            await self.client0.query('select median_low(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 531.0],
                [1447249051000000000, 532.0],
                [1447249052000000000, 54.0],
                [1447249053000000000, 528.0],
                [1447249054000000000, 534.0],
                [1447249055000000000, 533.0]
            ]})

        self.assertEqual(
            await self.client0.query('select median_high(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 534.0],
                [1447249051000000000, 532.0],
                [1447249052000000000, 54.0],
                [1447249053000000000, 531.0],
                [1447249054000000000, 534.0],
                [1447249055000000000, 533.0]
            ]})

        self.assertEqual(
            await self.client0.query('select min(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 531.0],
                [1447249051000000000, 530.0],
                [1447249052000000000, 54.0],
                [1447249053000000000, 513.0],
                [1447249054000000000, 533.0],
                [1447249055000000000, 532.0]
            ]})

        self.assertEqual(
            await self.client0.query('select max(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 535.0],
                [1447249051000000000, 537.0],
                [1447249052000000000, 520.0],
                [1447249053000000000, 537.0],
                [1447249054000000000, 538.0],
                [1447249055000000000, 537.0]
            ]})

        self.assertAlmostEqual(
            await self.client0.query('select variance(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 4.25],
                [1447249051000000000, 13.0],
                [1447249052000000000, 72385.3333333333333333333],
                [1447249053000000000, 104.25],
                [1447249054000000000, 7.0],
                [1447249055000000000, 7.0]
            ]})

        self.assertAlmostEqual(
            await self.client0.query('select pvariance(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 3.1875],
                [1447249051000000000, 8.666666666666666667],
                [1447249052000000000, 48256.8888888888888888887],
                [1447249053000000000, 78.1875],
                [1447249054000000000, 4.666666666666666667],
                [1447249055000000000, 4.666666666666666667]
            ]})

        self.assertEqual(
            await self.client0.query('select difference(1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 0],
                [1447249051000000000, -2],
                [1447249052000000000, -466],
                [1447249053000000000, 18],
                [1447249054000000000, 1],
                [1447249055000000000, 5]
            ]})

        self.assertAlmostEqual(
            await self.client0.query('select derivative(1, 1s) from "aggr"'),
            {'aggr': [
                [1447249050000000000, 0.0],
                [1447249051000000000, -0.000000002],
                [1447249052000000000, -0.000000466],
                [1447249053000000000, 0.000000018],
                [1447249054000000000, 0.000000001],
                [1447249055000000000, 0.000000005]
            ]})

        self.assertEqual(
            await self.client0.query('select filter(>534) from "aggr"'),
            {'aggr': [
                [1447249049633000000, 535],
                [1447249050549000000, 537],
                [1447249052349000000, 537],
                [1447249053549000000, 538],
                [1447249054748000000, 537]
            ]})

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
            {'aggr': [[1447249054748000000, 9674]]})

        # The interval over which the mean is calculated is obtained
        # by dividing the time period of the serie by the max_points
        # and adding 1 ns to include the last point of the series.
        self.assertEqual(
            await self.client0.query('select limit(3, mean) from "aggr"'),
            {'aggr': [
                [1447249050938000000, 532.8571428571429],
                [1447249052843000001, 367.6666666666667],
                [1447249054748000002, 534.0]]})

        self.assertEqual(
            await self.client0.query(
                'select limit(2, max)  from "series-001 float"'),
            {'series-001 float': [
                [1471254705000000007, 1.5],
                [1471254705000000013, -7.3]
            ]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select variance(1471254705000000012) from "variance"'),
            {'variance': [[1471254705000000012, 1.3720238095238095]]})

        self.assertAlmostEqual(
            await self.client0.query(
                'select pvariance(1471254705000000015) from "pvariance"'),
            {'pvariance': [[1471254705000000015, 1.25]]})

        self.assertEqual(
            await self.client0.query('select * from "one"'),
            {'one': [[1471254705000000010, 1]]})

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
                    [1471254705000000010, 'log line one'],
                    [1471254705000000016, 'and yet one more']],
                '2-log': [[1471254705000000012, 'log line two']]
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
                'Cannot use mean\(\) on string type\.'):
            await self.client0.query('select mean(1w) from "log"')

        with self.assertRaisesRegex(
                QueryError,
                'Group by time must be an integer value larger than zero\.'):
            await self.client0.query('select mean(0) from "aggr"')

        with self.assertRaisesRegex(
                QueryError,
                'Limit must be an integer value larger than zero\.'):
            await self.client0.query('select limit(6 - 6, mean) from "aggr"')

        with self.assertRaisesRegex(
                QueryError,
                'Cannot use a string filter on number type\.'):
            await self.client0.query(
                'select * from "aggr" '
                'merge as "t" using filter("0")')

        with self.assertRaisesRegex(
                QueryError,
                'Cannot use difference\(\) on string type\.'):
            await self.client0.query('select difference() from "log"')

        with self.assertRaisesRegex(
                QueryError,
                'Cannot use derivative\(\) on string type\.'):
            await self.client0.query('select derivative(6, 3) from "log"')

        with self.assertRaisesRegex(
                QueryError,
                'Cannot use derivative\(\) on string type\.'):
            await self.client0.query('select derivative() from "log"')

        with self.assertRaisesRegex(
                QueryError,
                'Overflow detected while using sum\(\)\.'):
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
                'select * from "aggr" where length > {}'.format('(' * 500))

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
                'select min(2s) prefix "min-", max(1s) prefix "max-" '
                'from /.*/ where type == integer and name != "filter" '
                'and name != "one" and name != "series-002 integer" '
                'merge as "int_min_max" using median_low(1) => difference()'),
            {
                'max-int_min_max': [
                    [1447249051000000000, 2],
                    [1447249052000000000, -17],
                    [1447249053000000000, 17],
                    [1447249054000000000, 1],
                    [1447249055000000000, -1],
                    [1471254706000000000, -532]
                ],
                'min-int_min_max': [
                    [1447249052000000000, -477],
                    [1447249054000000000, 459],
                    [1447249056000000000, 19],
                    [1471254706000000000, -532]
                ]})

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
            await self.client0.query('select stddev(1s) from "aggr"'),
            {"aggr": [
                [1447249050000000000, 2.0615528128088333333],
                [1447249051000000000, 3.6055512754639999999],
                [1447249052000000000, 269.0452254423666666667],
                [1447249053000000000, 10.2102889283311111111],
                [1447249054000000000, 2.6457513110645999999],
                [1447249055000000000, 2.6457513110645999999]
            ]})

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

        # return False


if __name__ == '__main__':
    parse_args()
    run_test(TestSelectNano())
