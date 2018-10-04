#!/usr/bin/python3
import os
import sys
import argparse
import asyncio
import time
import logging
import string
import random
import datetime
import math
import collections
import signal
from siridb.connector import SiriDBClient


# Version information
__version_info__ = (0, 0, 5)
__version__ = '.'.join(map(str, __version_info__))
__maintainer__ = 'Jeroen van der Heijden'
__email__ = 'jeroen@transceptor.technology'


# Logging definitions
_LOG_DATE_FMT = '%y%m%d %H:%M:%S'
_MAP_LOGLEVELS = {
    'DEBUG': logging.DEBUG,
    'INFO': logging.INFO,
    'WARNING': logging.WARNING,
    'ERROR': logging.ERROR,
    'CRITICAL': logging.CRITICAL
}

# counters etc.
total_processed = 0
total_failed = 0
start_time = time.time()

# Stop is used when pressing CTRL+C
stop = False


def setup_logger(args):
    # setup formatter without using colors
    formatter = logging.Formatter(
        fmt='[%(levelname)1.1s %(asctime)s %(module)s:%(lineno)d] ' +
            '%(message)s',
        datefmt=_LOG_DATE_FMT,
        style='%')

    logger = logging.getLogger()

    logger.setLevel(_MAP_LOGLEVELS[args.log_level.upper()])

    if args.log_file_prefix:
        # create file handler
        ch = logging.RotatingFileHandler(
            args.log_file_prefix,
            maxBytes=args.log_file_max_size,
            backupCount=args.log_file_num_backups)

    else:
        # create console handler
        ch = logging.StreamHandler()

    # we can set the handler level to DEBUG since we control the root level
    ch.setLevel(logging.DEBUG)
    ch.setFormatter(formatter)
    logger.addHandler(ch)


class Series:
    # Class values are set with the .init() method.
    _series = []
    _timestamp = None  # always in seconds
    _ts_factor = None
    _interval_range = None
    _r = None

    def __init__(self, r, allowed_kinds=(int, float, str), wrong_type=False):
        self.kind = r.choice(allowed_kinds)
        self.lval = {
            str: '',
            int: 0,
            float: 0.0
        }[self.kind]
        self.lts = self._timestamp

        factor = 10**r.randint(int(self.kind == int), 9)
        self.random_range = (
            int(r.random() * -factor),
            int(r.random() * factor) + 1)
        self.sign = 1

        self.ignore_last = r.random() > 0.95
        self.likely_equal = r.choice([0.01, 0.1, 0.2, 0.5, 0.99])
        self.likely_change_sign = r.choice([0.0, 0.1, 0.25, 0.5, 0.9])

        self.as_int = wrong_type and self.kind == float and r.random() > 0.9
        self.likely_inf = r.random() * 0.2 \
            if self.kind == float and r.random() > 0.95 else False
        self.likely_nan = r.random() * 0.2 \
            if self.kind == float and r.random() > 0.95 else False

        self.gen_float = wrong_type and self.kind == int and r.random() > 0.97

        self.name = self._gen_name()
        Series._series.append(self)

    def get_value(self):
        if self._r.random() < self.likely_change_sign:
            self.sign = -self.sign

        if self._r.random() > self.likely_equal:
            if self.kind == int:
                val = self.sign * self._r.randint(*self.random_range)
                if self.ignore_last:
                    self.lval = val
                else:
                    self.lval += val
                if self.lval.bit_length() > 63:
                    self.lval = 0

            elif self.kind == float:
                if self.likely_inf and self._r.random() < self.likely_inf:
                    return self.sign * math.inf
                elif self.likely_nan and self._r.random() < self.likely_nan:
                    return math.nan
                else:
                    val = self.sign * self._r.random() * self.random_range[1]
                    if self.ignore_last:
                        self.lval = val
                    else:
                        self.lval += val
                    if self.as_int:
                        self.lval = round(self.lval, 0)

        if self.gen_float:
            self.kind = float
            self.gen_float = False

        return self.lval

    @classmethod
    def init(cls, args, ts_factor):
        cls._r = random.Random()
        cls._r.seed(
            time.time() if args.seed_data is None else args.seed_data)

        series_rand = random.Random()
        series_rand.seed(
            time.time() if args.seed_series is None else args.seed_series)

        cls._ts_factor = ts_factor
        cls._timestamp = int(time.mktime(datetime.datetime.strptime(
            args.start_date,
            '%Y-%m-%d').timetuple()))

        n = math.ceil(args.ts_interval * 0.05)
        cls._interval_range = (-n, n)

        translate = {
            'int': int,
            'float': float,
            'str': str}
        kinds = [translate[k] for k in args.kinds]

        for i in range(args.num_series):
            Series(
                r=series_rand,
                allowed_kinds=kinds,
                wrong_type=args.wrong_type)

    def _gen_name(self):
        name = '/n:{}/range:{},{}/eq:{}/cs:{}/opt:{}{}{}{}{}'.format(
            len(self._series),
            self.random_range[0],
            self.random_range[1],
            self.likely_equal,
            self.likely_change_sign,
            '(ign_last)' if self.ignore_last else '',
            '(as_int)' if self.as_int else '',
            '(gen_float)' if self.gen_float else '',
            '(inf:{:.3f})'.format(self.likely_inf) if self.likely_inf else '',
            '(nan:{:.3f})'.format(self.likely_nan) if self.likely_nan else '')

        return name

    def get_ts(self, now, args):
        if args.ts_interval <= 0:
            return self._r.randint(self._timestamp, now) * self._ts_factor
        self.lts += args.ts_interval
        if args.ts_randomize:
            self.lts += self._r.randint(*self._interval_range)
        return self.lts * self._ts_factor

    @classmethod
    def pick_series(cls, n):
        series = cls._series[:]
        cls._r.shuffle(series)
        return series[:n]

    @classmethod
    def get_data(cls, args):
        nseries, npoints = args.series_per_batch, args.points_per_batch

        if nseries < 1:
            nseries = cls._r.randint(1, min(10000, cls.num()))

        if npoints < 1:
            npoints = cls._r.randint(nseries, 10000)

        series = cls.pick_series(nseries)
        now = int(time.time())
        data = collections.defaultdict(list)

        for s in series:
            val = s.get_value()
            ts = s.get_ts(now, args)
            data[s.name].append([ts, val])

        npoints -= nseries
        while npoints:
            s = cls._r.choice(series)
            ts = s.get_ts(now, args)
            val = s.get_value()
            data[s.name].append([ts, val])
            npoints -= 1

        return data

    @classmethod
    def num(cls):
        return len(cls._series)


async def get_ts_factor(siri):
    res = await siri.query('show time_precision')
    return 10**(['s', 'ms', 'us', 'ns'].index(res['data'][0]['value'])*3)


def queue_data(args):
    n = args.num_batches
    while n and stop is False:
        data = Series.get_data(args)
        yield data
        n -= 1


async def siridb_insert(siri, data, task_counter):
    '''Insert data into SiriDB.'''
    global total_processed
    global total_failed
    global start_time

    n = sum((len(p) for p in data.values()))
    start = time.time()
    try:
        await siri.insert(data)
        # await asyncio.sleep(0.1)
    except Exception as e:
        logging.exception(e)
        total_failed += n
    else:
        total_processed += n
        logging.info(
            'processed {} records, running {} task(s), {:d} records/sec'
            .format(
                total_processed,
                len(task_counter) - 1,
                int(total_processed // (time.time() - start_time))))
    finally:
        task_counter.pop()


async def dump_data(siri, args):
    task_counter = []

    try:
        await siri.connect()

        ts_factor = await get_ts_factor(siri)
        Series.init(args, ts_factor)

        q = queue_data(args)
        for data in q:

            task_counter.append(1)
            while len(task_counter) > args.max_parallel:
                await asyncio.sleep(0.2)

            asyncio.ensure_future(siridb_insert(siri, data, task_counter))

            # sleep 0 so the async loop will run to pick-up tasks
            await asyncio.sleep(args.sleep)

    except Exception as e:
        logging.exception(e)

    finally:
        while len(task_counter):
            await asyncio.sleep(0.5)
        siri.close()


def signal_handler(signal, frame):
    global stop
    logging.warning(
        'Asked to stop, existing data in the queue will be finished...')
    stop = True


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-u', '--user',
        type=str,
        default='iris',
        help='database user')

    parser.add_argument(
        '-p', '--password',
        type=str,
        default='siri',
        help='password')

    parser.add_argument(
        '-d', '--dbname',
        type=str,
        default='dbtest',
        help='database name')

    parser.add_argument(
        '-s', '--servers',
        type=str,
        default='localhost:9000',
        help='siridb server(s)')

    parser.add_argument(
        '--seed-series',
        type=str,
        help='Optional seed for generating series. '
             'If no seed is given, the current timestamp will be used.')

    parser.add_argument(
        '--seed-data',
        type=str,
        help='Optional seed for generating data. '
             'If no seed is given, the current timestamp will be used.')

    parser.add_argument(
        '-v', '--version',
        action='store_true',
        help='print version information and exit')

    parser.add_argument(
        '--start-date',
        type=str,
        default='2017-01-01',
        help='Timestamps will be generated starting from this date. '
             '(using the format YYYY-MM-DD')

    parser.add_argument(
        '--ts-interval',
        type=int,
        default=60,
        help='Timestamp interval in seconds. '
             'When <= 0 a ramdom interval is used for each data point.')

    parser.add_argument(
        '--sleep',
        type=float,
        default=0.0,
        help='Sleep between inserts in seconds. (default: 0.0 seconds)')

    parser.add_argument(
        '--ts-randomize',
        action='store_true',
        help='A random value will be added to the timestamp intervals so '
             'they will be not exact anymore. In case --ts-interval is '
             '<= 0 this property will be ignored.')

    parser.add_argument(
        '--num-series',
        type=int,
        default=10000,
        help='number of series to generate')

    parser.add_argument(
        '--num-batches',
        type=int,
        default=10,
        help='number of batches to inserts. (value < 0 will be continues')

    parser.add_argument(
        '--series-per-batch',
        type=int,
        default=5000,
        help='number of series per batch. (0 for random)')

    parser.add_argument(
        '--points-per-batch',
        type=int,
        default=10000,
        help='number of points per batch. '
             '(must be equal or larger than series-per-batch or 0 for random)')

    parser.add_argument(
        '--kinds',
        nargs='+',
        default=('int', 'float'),
        choices=('int', 'float'))  # , 'str'

    parser.add_argument(
        '--wrong-type',
        action='store_true',
        help='Allow series to insert points using a wrong type')

    parser.add_argument(
        '--max-parallel',
        type=int,
        default=3,
        help='maximum number of allowed parallel inserts')

    parser.add_argument(
        '-l', '--log-level',
        default='info',
        help='set the log level (default: info)',
        choices=['debug', 'info', 'warning', 'error'])

    parser.add_argument(
        '--log-file-max-size',
        default=50000000,
        help='max size of log files before rollover ' +
        '(--log-file-prefix must be set)',
        type=int)

    parser.add_argument(
        '--log-file-num-backups',
        default=6,
        help='number of log files to keep (--log-file-prefix must be set)',
        type=int)

    parser.add_argument(
        '--log-file-prefix',
        help='path prefix for log files (when not provided we send the ' +
        'output to the console)',
        type=str)

    args = parser.parse_args()

    # respond to --version argument
    if args.version:
        sys.exit('''
SiriDB Data Generator Script {version}
Maintainer: {maintainer} <{email}>
Home-page: https://github.com/transceptor-technology/siridb-email-check
        '''.strip().format(version=__version__,
                           maintainer=__maintainer__,
                           email=__email__))

    if args.num_series < args.series_per_batch:
        exit('num-series must be equal or greater than series-per-batch')

    if args.points_per_batch != 0 and \
            args.points_per_batch < args.series_per_batch:
        exit('points-per-batch must be equal or greater than series-per-batch')

    if args.max_parallel < 1:
        exit('max-parallel must be > 0')

    try:
        if datetime.datetime.strptime(
                args.start_date, '%Y-%m-%d') >= datetime.datetime.today():
            exit('start date must be a date before today.')
    except Exception:
        exit('invalid date: {}'.format(args.start_date))

    setup_logger(args)
    signal.signal(signal.SIGINT, signal_handler)

    siri = SiriDBClient(
        username=args.user,
        password=args.password,
        dbname=args.dbname,
        hostlist=[
            [s.strip() for s in server.split(':')]
            for server in args.servers.split(',')])

    loop = asyncio.get_event_loop()
    loop.run_until_complete(dump_data(siri, args))

    total_time = time.time() - start_time
    logging.info(
        'total time: {:.3f} seconds, '
        'processed: {}, '
        'failed: {}'
        .format(
            total_time,
            total_processed,
            total_failed))
