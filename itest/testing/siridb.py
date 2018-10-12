import os
import logging
import random
import asyncio
from .constants import SERVICE

VERBOSE = ' --verbose'


class SiriDB:

    LOG_LEVEL = 'CRITICAL'

    def __init__(self,
                 dbname='dbtest',
                 time_precision='s',
                 buffer_path='',
                 duration_log='1d',
                 duration_num='1w',
                 buffer_size=1024,
                 **unused):
        self.dbname = dbname
        self.time_precision = time_precision
        self.buffer_path = buffer_path
        self.duration_log = duration_log
        self.duration_num = duration_num
        self.buffer_size = buffer_size
        self.servers = []

    async def create_on(self, server, sleep=None):
        await asyncio.sleep(1)

        logging.info('Create database {} on {}'.format(
            self.dbname,
            server.name))

        rc = os.system(
            '{service} '
            '-u sa -p siri -s {addr} '
            'new-database '
            '--db-name {dbname} '
            '--time-precision {time_precision} '
            '--duration-log {duration_log} '
            '--duration-num {duration_num} '
            '--buffer-size {buffer_size}'
            '{verbose}'.format(
                service=SERVICE,
                addr=server.addr,
                verbose=VERBOSE if self.LOG_LEVEL == 'DEBUG'
                else ' >/dev/null',
                **vars(self)))

        assert rc == 0, 'Expected rc = 0 but got rc = {}'.format(rc)

        self.servers.append(server)

        if sleep:
            await asyncio.sleep(sleep)

    async def add_replica(
                self,
                server,
                pool,
                username='iris',
                password='siri',
                remote_server=None,
                sleep=None):

        if remote_server is None:
            remote_server = random.choice(self.servers)

        rc = os.system(
            '{service} '
            '-u sa -p siri -s {addr} '
            'new-replica '
            '--db-name {dbname} '
            '--db-server {dbaddr} '
            '--db-user {dbuser} '
            '--db-password {dbpassword} '
            '--pool {pool} '
            '--force{verbose}'.format(
                service=SERVICE,
                addr=server.addr,
                dbaddr=remote_server.addr,
                dbuser=username,
                dbpassword=password,
                pool=pool,
                verbose=VERBOSE if self.LOG_LEVEL == 'DEBUG'
                else ' >/dev/null',
                **vars(self)))

        assert rc == 0, 'Expected rc = 0 but got rc = {}'.format(rc)

        self.servers.append(server)

        if sleep:
            await asyncio.sleep(sleep)

    async def add_pool(
                self,
                server,
                username='iris',
                password='siri',
                remote_server=None,
                sleep=None):

        if remote_server is None:
            remote_server = random.choice(self.servers)

        rc = os.system(
            '{service} '
            '-u sa -p siri -s {addr} '
            'new-pool '
            '--db-name {dbname} '
            '--db-server {dbaddr} '
            '--db-user {dbuser} '
            '--db-password {dbpassword} '
            '--force{verbose}'.format(
                service=SERVICE,
                addr=server.addr,
                dbaddr=remote_server.addr,
                dbuser=username,
                dbpassword=password,
                verbose=VERBOSE if self.LOG_LEVEL == 'DEBUG'
                else ' >/dev/null',
                **vars(self)))

        assert rc == 0, 'Expected rc = 0 but got rc = {}'.format(rc)

        self.servers.append(server)

        if sleep:
            await asyncio.sleep(sleep)
