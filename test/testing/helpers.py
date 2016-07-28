import os
import logging
import asyncio
import shutil
import time
from .constants import TEST_DIR
from .testbase import TestBase

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
        print('{:.<76}FAILED ({:.2f} ms)'.format(
            test.title,
            time.time() - start))
        raise e
    else:
        print('{:.<80}OK ({:.2f} ms)'.format(
            test.title,
            time.time() - start))

    logger.setLevel('CRITICAL')


def run_test(test, loglevel='CRITICAL'):
    assert isinstance(test, TestBase)
    loop = asyncio.get_event_loop()
    cleanup()
    loop.run_until_complete(_run_test(test, loglevel))