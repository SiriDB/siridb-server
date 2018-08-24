import os
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
from testing import SiriDBAsyncUnixConnection

PIPE_NAME = '/tmp/siridb_pipe_test.sock'

DATA = {
    'series num_float': [
        [1471254705, 1.5],
        [1471254707, -3.5],
        [1471254710, -7.3]],
    'series num_integer': [
        [1471254705, 5],
        [1471254708, -3],
        [1471254710, -7]],
    'series_log': [
        [1471254710, 'log line one'],
        [1471254712, 'log line two'],
        [1471254714, 'another line (three)'],
        [1471254716, 'and yet one more']]
}

if os.path.exists(PIPE_NAME):
    os.unlink(PIPE_NAME)

class TestPipeSupport(TestBase):
    title = 'Test pipe support object'


    @default_test_setup(1, pipe_name=PIPE_NAME)
    async def run(self):

        pipe_client = SiriDBAsyncUnixConnection(PIPE_NAME)

        await pipe_client.connect('iris', 'siri', self.db.dbname)

        await pipe_client.insert(DATA)

        self.assertEqual(
            await pipe_client.query('select * from "series_log"'),
            {'series_log': DATA['series_log']})

        pipe_client.close()


        # return False


if __name__ == '__main__':
    SiriDB.LOG_LEVEL = 'CRITICAL'
    Server.HOLD_TERM = True
    Server.MEM_CHECK = True
    Server.BUILDTYPE = 'Debug'
    run_test(TestPipeSupport())
