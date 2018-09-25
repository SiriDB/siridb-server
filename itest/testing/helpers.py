import os
import logging
import shutil
import time
import random
import functools
import string
from .constants import TEST_DIR
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

    logging.info('Force kill all open siridb-server processes')
    os.system('pkill -9 siridb-server')

    logging.info('Force kill all open memcheck-amd64- processes')
    os.system('pkill -9 memcheck-amd64-')


def random_value(tp=float, mi=-100, ma=100):
    i = random.randrange(mi, ma)
    if tp == float:
        return i * random.random()
    elif tp == int:
        return i


def random_series_name(size=12, chars=string.ascii_letters + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))


def gen_points(
        n=100,
        time_precision='s',
        tp=float,
        mi=-100,
        ma=100,
        ts_gap=1,
        shuffle=True):
    if isinstance(ts_gap, str):
        if ts_gap.endswith('s'):
            ts_gap = int(ts_gap[:-1]) * _MAP_TS[time_precision]
        elif ts_gap.endswith('m'):
            ts_gap = int(ts_gap[:-1]) * _MAP_TS[time_precision] * 60
        elif ts_gap.endswith('h'):
            ts_gap = int(ts_gap[:-1]) * _MAP_TS[time_precision] * 3600
        elif ts_gap.endswith('d'):
            ts_gap = int(ts_gap[:-1]) * _MAP_TS[time_precision] * 3600 * 24
        elif ts_gap.endswith('w'):
            ts_gap = int(ts_gap[:-1]) * _MAP_TS[time_precision] * 3600 * 24 * 7

    end = int(time.time() * _MAP_TS[time_precision])
    start = end - (n * ts_gap)
    timestamps = list(range(start, end, ts_gap))
    if shuffle:
        random.shuffle(timestamps)
    return [[ts, random_value(tp, mi, ma)] for ts in timestamps]


def gen_data(points=functools.partial(gen_points), n=100):
    return {random_series_name(): points() for _ in range(n)}


def gen_series(n=10000):
    return [Series(random_series_name()) for _ in range(n)]
