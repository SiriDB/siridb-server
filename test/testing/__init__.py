import sys
import asyncio
import logging
import time
from .client import Client
from .client import InsertError
from .client import PoolError
from .client import QueryError
from .client import ServerError
from .client import UserAuthError
from .helpers import cleanup
from .helpers import gen_data
from .helpers import gen_points
from .helpers import gen_series
from .server import Server
from .siridb import SiriDB
from .testbase import default_test_setup
from .testbase import TestBase
from .series import Series
from .pipe_client import PipeClient as SiriDBAsyncUnixConnection


async def _run_test(test, loglevel):
    logger = logging.getLogger()
    logger.setLevel(loglevel)

    start = time.time()
    print('{:.<76}'.format(test.title), end='')
    sys.stdout.flush()

    try:
        await test.run()
    except Exception as e:
        print('FAILED ({:.2f} seconds)'.format(time.time() - start))
        raise e
    else:
        print('OK ({:.2f} seconds)'.format(time.time() - start))

    logger.setLevel('CRITICAL')


def run_test(test, loglevel='CRITICAL'):
    assert isinstance(test, TestBase)
    loop = asyncio.get_event_loop()
    cleanup()
    loop.run_until_complete(_run_test(test, loglevel))
