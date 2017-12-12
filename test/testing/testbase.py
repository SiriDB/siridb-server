import unittest
import time
import asyncio
import random
import collections
from .siridb import SiriDB
from .server import Server
from .client import Client


def default_test_setup(nservers=1, **kwargs):
    def wrapper(func):
        async def wrapped(self):
            self.db = SiriDB(**kwargs)

            self.servers = [Server(n, **kwargs) for n in range(nservers)]
            for n, server in enumerate(self.servers):
                setattr(self, 'server{}'.format(n), server)
                setattr(self, 'client{}'.format(n), Client(self.db, server))
                server.create()
                await server.start()

            time.sleep(2.0)

            await self.db.create_on(self.server0, sleep=2)

            close = await func(self)

            if close or close is None:
                for server in self.servers:
                    result = await server.stop()
                    self.assertTrue(
                        result,
                        msg='Server {} did not close correctly'.format(
                            server.name))

        return wrapped
    return wrapper


class TestBase(unittest.TestCase):

    title = 'No title set'

    async def run():
        raise NotImplementedError()

    async def assertIsRunning(self, db, client, timeout=None):
        while True:
            result = await client.query('list servers name, status')
            result = result['servers']
            try:
                assert len(result) == len(self.db.servers), \
                    'Server(s) are missing: {} (expexting: {})'.format(
                        result, self.db.servers)
            except AssertionError as e:
                if not timeout:
                    raise e
            else:
                try:
                    assert all([
                        status == 'running' for name, status in result]), \
                        'Not all servers have status running: {}'.format(
                            result)
                except AssertionError as e:
                    if not timeout:
                        raise e
                else:
                    break

            if timeout is not None:
                timeout -= 1

            await asyncio.sleep(1.0)

    async def assertSeries(self, client, series):
        d = {s.name: s.points for s in series if s.points}

        result = await client.query('list series name, length limit {}'.format(
            len(series)))
        result = {name: length for name, length in result['series']}
        for s in series:
            if s.points:
                length = result.get(s.name, None)
                assert length is not None, \
                    'series {!r} is missing in the result'.format(s.name)
                assert length == len(s.points) or \
                    s.commit_points() or \
                    length == len(s.points), \
                    'expected {} point(s) but found {} point(s) ' \
                    'for series {!r}' \
                    .format(len(s.points), length, s.name)

        n = min(len(series), 10)
        l = list(d.keys())
        random.shuffle(l)
        for i in range(n):
            result = await client.query('select * from "{}"'.format(l[i]))
            points = result[l[i]]
            expected = sorted(d[l[i]])
            assert len(points) == len(expected), \
                'incorrect number of points: {}, expected: {}'.format(
                    len(points), len(expected))
            c = collections.Counter([p[0] for p in points])
            for i, p in enumerate(points):
                ts, val = p
                if c[ts] == 1:
                    self.assertEqual(p, expected[i])

    def assertAlmostEqual(self, a, b, *args, **kwargs):
        if isinstance(b, dict):
            for series, points in b.items():
                assert isinstance(points, list), 'Expecting a list of points'
                for i, point in enumerate(points):
                    assert isinstance(point, list) and len(point) == 2, \
                        'Expecting a point to be a list of 2 items'
                    super().assertEqual(a[series][i][0], point[0])
                    super().assertAlmostEqual(
                        a[series][i][1],
                        point[1],
                        *args,
                        **kwargs)
        else:
            super().assertAlmostEqual(a, b, *args, **kwargs)



