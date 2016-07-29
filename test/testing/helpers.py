import os
import logging
import asyncio
import shutil
import time
import random
import functools
import string
from .constants import TEST_DIR
from .testbase import TestBase
from .series import Series

_MAP_TS = {
    's': 10**0,
    'ms': 10**3,
    'us': 10**6,
    'ns': 10**9
}

def cleanup():
    logging.info('Remove test dir')
    try:
        shutil.rmtree(os.path.join(TEST_DIR))
    except FileNotFoundError:
        pass
    os.mkdir(TEST_DIR)

    logging.info('Force kill all open siridbc processes')
    os.system('pkill -9 siridbc')

async def _run_test(test, loglevel):
    logger = logging.getLogger()
    logger.setLevel(loglevel)

    start = time.time()
    try:
        await test.run()
    except Exception as e:
        print('{:.<76}FAILED ({:.2f} seconds)'.format(
            test.title,
            time.time() - start))
        raise e
    else:
        print('{:.<80}OK ({:.2f} seconds)'.format(
            test.title,
            time.time() - start))

    logger.setLevel('CRITICAL')

def random_value(tp=float, mi=-100, ma=100):
    i = random.randrange(mi, ma)
    if tp == float:
        return i * random.random()
    elif tp == int:
        return i

def random_series_name(size=12, chars=string.ascii_letters + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))

def gen_points(n=100, time_precision='s', tp=float, mi=-100, ma=100):
    start = int(time.time() * _MAP_TS[time_precision]) - n
    return [[ts, random_value(tp, mi, ma)] for ts in range(start, start + n)]

def gen_data(points=functools.partial(gen_points), n=100):
    return {random_series_name(): points() for _ in range(n)}

def gen_series(n=10000):
    return [Series(random_series_name()) for _ in range(n)]

def some_series(series, n=None, points=functools.partial(gen_points, n=1)):
    random.shuffle(series)
    if n is None:
        n = len(series) // 100
    return {s.name: s.add_points(points()) for s in series[:n]}

def run_test(test, loglevel='CRITICAL'):
    assert isinstance(test, TestBase)
    loop = asyncio.get_event_loop()
    cleanup()
    loop.run_until_complete(_run_test(test, loglevel))