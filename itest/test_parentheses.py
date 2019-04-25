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


LENPOINTS = 36
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
    'linux-001': [
        [1471254705000000005, 7.3],
        [1471254705000000007, -6.4],
        [1471254705000000010, -9.8]],
    'linux-002': [
        [1471254705000000005, 2],
        [1471254705000000008, -7],
        [1471254705000000010, -9]],
    'linux-003': [
        [1471254705000000005, 2.9],
        [1471254705000000007, -5.7],
        [1471254705000000010, -0.3]],
    'linux-004': [
        [1471254705000000005, 3],
        [1471254705000000008, -9],
        [1471254705000000010, -8]],
    'windows-001': [
        [1471254705000000005, 9.3],
        [1471254705000000007, -3.3],
        [1471254705000000010, -1.6]],
    'windows-002': [
        [1471254705000000005, 4],
        [1471254705000000008, -8],
        [1471254705000000010, -2]],
    'windows-003': [
        [1471254705000000005, 4.3],
        [1471254705000000007, -7.9],
        [1471254705000000010, -1.2]],
    'windows-004': [
        [1471254705000000005, 2],
        [1471254705000000008, -5],
        [1471254705000000010, -7]],

}

TIME_PRECISION = 'ns'


class TestParenth(TestBase):
    title = 'Test parentheses'

    @default_test_setup(1, time_precision=TIME_PRECISION)
    async def run(self):
        await self.client0.connect()

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Successfully inserted {} point(s).'.format(
                LENPOINTS)})

        result = await self.client0.query('''
                list series
                    all - ("series-001" | "series-002" | /windows.*/)
               ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-003'],
                    ['series-004'],
                    ['linux-001'],
                    ['linux-002'],
                    ['linux-003'],
                    ['linux-004']
                ])
            }
        )

        result = await self.client0.query('''
                list series all - (
                    "series-001" | "series-002" | (/windows.*/ & /.*001/))
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-003'],
                    ['series-004'],
                    ['linux-001'],
                    ['linux-002'],
                    ['linux-003'],
                    ['linux-004'],
                    ['windows-002'],
                    ['windows-003'],
                    ['windows-004']
                ])
            }
        )
        result = await self.client0.query('''
                list series all - (
                    "series-001" | "series-002" | (/windows.*/ - /.*001/))
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-003'],
                    ['series-004'],
                    ['linux-001'],
                    ['linux-002'],
                    ['linux-003'],
                    ['linux-004'],
                    ['windows-001']
                ])
            }
        )

        result = await self.client0.query('''
                list series (
                    "series-001" | "series-002" | /windows.*/) - /.*003/
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-001'],
                    ['series-002'],
                    ['windows-001'],
                    ['windows-002'],
                    ['windows-004']
                ])
            }
        )

        result = await self.client0.query('''
                list series all - (/series.*/ ^ /.*001/)
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-001'],
                    ['linux-002'],
                    ['linux-003'],
                    ['linux-004'],
                    ['windows-002'],
                    ['windows-003'],
                    ['windows-004']
                ])
            }
        )

        self.assertEqual(
            await self.client0.query('''
                list series (/.*001/ & /linux.*/) - /.*001/
                '''),
            {
                'columns': ['name'],
                'series': []})

        result = await self.client0.query('''
                list series /.*001/ & (/series.*/ | /linux.*/)
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-001'],
                    ['linux-001']
                ])
            }
        )

        result = await self.client0.query('''
                list series /.*001/ & ((((/series.*/ | /linux.*/))))
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-001'],
                    ['linux-001']
                ])
            }
        )

        result = await self.client0.query('''
                list series (/.*001/ | /.*002/) & (/series.*/ | /linux.*/)
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-001'],
                    ['series-002'],
                    ['linux-001'],
                    ['linux-002']
                ])
            }
        )

        result = await self.client0.query('''
                list series ((/.*001/ | /.*002/) & (/series.*/ | /linux.*/))
                ''')
        result['series'] = sorted(result['series'])

        self.assertEqual(
            result,
            {
                'columns': ['name'],
                'series': sorted([
                    ['series-001'],
                    ['series-002'],
                    ['linux-001'],
                    ['linux-002']
                ])
            }
        )

        with self.assertRaisesRegex(
                QueryError,
                'Query error at position 29. Expecting \*, all, '
                'single_quote_str, double_quote_str or \('):
            await self.client0.query(
                'list series /.*/ - {}{}'.format('(' * 10, ')' * 10))

        with self.assertRaisesRegex(
                QueryError,
                'Memory allocation error or maximum recursion depth reached.'):
            await self.client0.query('''
                list series /.*/ -
                    {}/linux.*/{}'''.format('(' * 500, ')' * 500))

        await self.client0.query('alter database set list_limit 5000')
        with self.assertRaisesRegex(
                QueryError,
                'Limit must be a value between 0 and 5000 '
                'but received: 6000.*'):
            await self.client0.query(
                'list series limit 6000')

        self.client0.close()


if __name__ == '__main__':
    parse_args()
    run_test(TestParenth())
