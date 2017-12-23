import os
import logging
import random
import asyncio
from .constants import MANAGE
from .constants import ADMIN
from .constants import TOOL

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
        logging.info('Create database {} on {}'.format(
            self.dbname,
            server.name))

        if TOOL == 'MANAGE':
            rc = os.system(
                '{manage} '
                '--log-level {log_level} '
                '--noroot --config {cfgfile} create-new '
                '--dbname {dbname} '
                '--time-precision {time_precision} '
                '{bufpath}'
                '--duration-log {duration_log} '
                '--duration-num {duration_num} '
                '--buffer-size {buffer_size}'.format(
                    manage=MANAGE,
                    log_level=self.LOG_LEVEL.lower(),
                    cfgfile=server.cfgfile,
                    bufpath='' if not self.buffer_path
                            else '--buffer-path {}'.format(self.buffer_path),
                    **vars(self)))
        elif TOOL == 'ADMIN':
            rc = os.system(
                '{admin} '
                '-u sa -p siri -s {addr} '
                'new-database '
                '--db-name {dbname} '
                '--time-precision {time_precision} '
                '--duration-log {duration_log} '
                '--duration-num {duration_num} '
                '--buffer-size {buffer_size}'
                '{verbose}'.format(
                    admin=ADMIN,
                    addr=server.addr,
                    verbose=VERBOSE if self.LOG_LEVEL == 'DEBUG'
                    else ' >/dev/null',
                    **vars(self)))
        else:
            logging.error(
                'TOOL should be either MANAGE or ADMIN, got: {}'
                .format(TOOL))
            rc = 1

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

        if TOOL == 'MANAGE':
            rc = os.system(
                '{manage} '
                '--log-level {log_level} '
                '--noroot --config {cfgfile} create-replica '
                '--dbname {dbname} '
                '--remote-address {remote_address} '
                '--remote-port {remote_port} '
                '--user {user} '
                '--password {password} '
                '--pool {pool} '
                '{bufpath}'
                '--buffer-size {buffer_size}'.format(
                    manage=MANAGE,
                    log_level=self.LOG_LEVEL.lower(),
                    cfgfile=server.cfgfile,
                    user=username,
                    password=password,
                    pool=pool,
                    bufpath='' if not self.buffer_path
                            else '--buffer-path {}'.format(self.buffer_path),
                    **vars(self),
                    remote_address=remote_server.server_address,
                    remote_port=remote_server.listen_client_port))
        elif TOOL == 'ADMIN':
            rc = os.system(
                '{admin} '
                '-u sa -p siri -s {addr} '
                'new-replica '
                '--db-name {dbname} '
                '--db-server {dbaddr} '
                '--db-user {dbuser} '
                '--db-password {dbpassword} '
                '--pool {pool} '
                '--force{verbose}'.format(
                    admin=ADMIN,
                    addr=server.addr,
                    dbaddr=remote_server.addr,
                    dbuser=username,
                    dbpassword=password,
                    pool=pool,
                    verbose=VERBOSE if self.LOG_LEVEL == 'DEBUG'
                    else ' >/dev/null',
                    **vars(self)))
        else:
            logging.error(
                'TOOL should be either MANAGE or ADMIN, got: {}'
                .format(TOOL))
            rc = 1

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

        if TOOL == 'MANAGE':
            rc = os.system(
                '{manage} '
                '--log-level {log_level} '
                '--noroot --config {cfgfile} create-pool '
                '--dbname {dbname} '
                '--remote-address {remote_address} '
                '--remote-port {remote_port} '
                '--user {user} '
                '--password {password} '
                '{bufpath}'
                '--buffer-size {buffer_size}'.format(
                    manage=MANAGE,
                    log_level=self.LOG_LEVEL.lower(),
                    cfgfile=server.cfgfile,
                    user=username,
                    password=password,
                    bufpath='' if not self.buffer_path
                            else '--buffer-path {}'.format(self.buffer_path),
                    **vars(self),
                    remote_address=remote_server.server_address,
                    remote_port=remote_server.listen_client_port))
        elif TOOL == 'ADMIN':
            rc = os.system(
                '{admin} '
                '-u sa -p siri -s {addr} '
                'new-pool '
                '--db-name {dbname} '
                '--db-server {dbaddr} '
                '--db-user {dbuser} '
                '--db-password {dbpassword} '
                '--force{verbose}'.format(
                    admin=ADMIN,
                    addr=server.addr,
                    dbaddr=remote_server.addr,
                    dbuser=username,
                    dbpassword=password,
                    verbose=VERBOSE if self.LOG_LEVEL == 'DEBUG'
                    else ' >/dev/null',
                    **vars(self)))
        else:
            logging.error(
                'TOOL should be either MANAGE or ADMIN, got: {}'
                .format(TOOL))
            rc = 1

        assert rc == 0, 'Expected rc = 0 but got rc = {}'.format(rc)

        self.servers.append(server)

        if sleep:
            await asyncio.sleep(sleep)


