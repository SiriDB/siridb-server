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
from .args import parse_args

SPINNER1 = \
    ('▁', '▂', '▃', '▄', '▅', '▆', '▇', '█', '▇', '▆', '▅', '▄', '▃', '▁')
SPINNER2 = \
    ('⠁', '⠂', '⠄', '⡀', '⢀', '⠠', '⠐', '⠈')
SPINNER3 = \
    ('◐', '◓', '◑', '◒')


class Spinner():

    def __init__(self, charset=SPINNER3):
        self._idx = 0
        self._charset = charset
        self._len = len(charset)

    @property
    def next(self):
        char = self._charset[self._idx]
        self._idx += 1
        self._idx %= self._len
        return char


class Task():
    def __init__(self, title):
        self.running = True
        self.task = asyncio.ensure_future(self.process())
        self.success = False
        self.title = title
        self.start = time.time()

    def stop(self, success):
        self.running = False
        self.success = success
        self.duration = time.time() - self.start

    async def process(self):
        spinner = Spinner()
        while self.running:
            sys.stdout.write(f'{self.title:.<76}{spinner.next}\r')
            sys.stdout.flush()
            await asyncio.sleep(0.2)

        if self.success:
            print(f'{self.title:.<76}OK ({self.duration:.2f} seconds)')
        else:
            print(f'{self.title:.<76}FAILED ({self.duration:.2f} seconds)')


async def _run_test(test, loglevel):
    logger = logging.getLogger()
    logger.setLevel(loglevel)
    task = Task(test.title)

    try:
        await test.run()
    except Exception as e:
        task.stop(success=False)
        raise e
    else:
        task.stop(success=True)

    logger.setLevel('CRITICAL')

    await task.task


def run_test(test, loglevel='CRITICAL'):
    assert isinstance(test, TestBase)
    loop = asyncio.get_event_loop()
    cleanup()
    loop.run_until_complete(_run_test(test, loglevel))
