import random
import time
from testing import Client
from testing import default_test_setup
from testing import gen_points
from testing import gen_series
from testing import InsertError
from testing import PoolError
from testing import run_test
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
}


class TestInsert(TestBase):
    title = 'Test select and aggregate functions'

    @default_test_setup(1)
    async def run(self):
        await self.client0.connect()

        self.assertEqual(
            await self.client0.insert(DATA),
            {'success_msg': 'Inserted 6 point(s) successfully.'})

        self.assertEqual(
            await self.client0.query(
                'select difference() from "series integer"'),
            {'series integer': [[1471254708, -8], [1471254710, -4]]})

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
                'select * from /.*/ merge as "median_low" using median_low({})'
                .format(now)),
            {'median_low': [[now, -3.5]]})

        self.assertEqual(
            await self.client0.query(
                'select * from /.*/ merge as "median_high" using median_high({})'
                .format(now)),
            {'median_high': [[now, -3.0]]})

        self.client0.close()


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = False
    run_test(TestInsert())
