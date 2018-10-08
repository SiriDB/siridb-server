import asyncio
import functools
import random
import time
import re
import calendar
import datetime
import collections
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


# Compression OFF:
# du --bytes testdir/dbpath0/dbtest/shards/
# 423114  testdir/dbpath0/dbtest/shards/
# 416266  (optimized)

# Compression ON:
# du --bytes testdir/dbpath0/dbtest/shards/
# 222314  testdir/dbpath0/dbtest/shards/
# 153380  (optimized)

SYSLOG = '/home/joente/syslog.log'
FMT = '%b %d %H:%M:%S'
MTCH = re.compile(
    '(\w\w\w\s[\d\s]\d\s\d\d:\d\d:'
    '\d\d)\s(\w+)\s([\w\-\.\/@]+)(\[\d+\])?:\s(.*)')


class TestSyslog(TestBase):
    title = 'Test with syslog data'

    async def insert_syslog(self, batch_size=100):

        with open(SYSLOG, 'r') as f:
            lines = f.readlines()

        points = collections.defaultdict(list)
        n = 0

        for line in lines:
            r = MTCH.match(line.strip())
            if not r:
                continue

            rtime, host, process, pid, logline = r.groups()
            dt = datetime.datetime.strptime(rtime, FMT)
            dt = dt.replace(year=datetime.datetime.now().year)
            ts = calendar.timegm(dt.timetuple())
            points['{}|{}'.format(host, process)].append([ts, logline])
            n += 1
            if n % batch_size == 0:
                await self.client0.insert(points)
                points.clear()

        if points:
            await self.client0.insert(points)

    @default_test_setup(2, compression=True, duration_log='1w')
    async def run(self):
        await self.client0.connect()

        # await self.db.add_pool(self.server1, sleep=30)

        await self.db.add_replica(self.server1, 0, sleep=30)

        await self.insert_syslog()

        await self.client0.query('select * from /.*vbox.*/ merge as "t"')

        self.client0.close()

        return False


if __name__ == '__main__':
    parse_args()
    run_test(TestSyslog())
