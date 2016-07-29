import os
import logging
import random
import asyncio
from .constants import MANAGE

class SiriDB:
    HOLD_TERM = False

    def __init__(self,
                 dbname='dbtest',
                 time_precision='s',
                 buffer_path=None,
                 duration_log='1d',
                 duration_num='1w',
                 buffer_size=1024):
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

        self.servers.append(server)
        rc = os.system(
            'xfce4-terminal -e "{manage} '
            '--noroot --config {cfgfile} create-new '
            '--dbname {dbname} '
            '--time-precision {time_precision} '
            '--buffer-path {buffer_path} '
            '--duration-log {duration_log} '
            '--duration-num {duration_num} '
            '--buffer-size {buffer_size}"{hold}'.format(
                manage=MANAGE,
                cfgfile=server.cfgfile,
                **vars(self),
                hold=' -H' if self.HOLD_TERM else ''))

        assert (rc == 0)

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

        self.servers.append(server)
        rc = os.system(
            'xfce4-terminal -e "{manage} '
            '--noroot --config {cfgfile} create-replica '
            '--dbname {dbname} '
            '--remote-address {remote_address} '
            '--remote-port {remote_port} '
            '--user {user} '
            '--password {password} '
            '--pool {pool} '
            '--buffer-path {buffer_path} '
            '--buffer-size {buffer_size}"{hold}'.format(
                manage=MANAGE,
                cfgfile=server.cfgfile,
                user=username,
                password=password,
                pool=pool,
                **vars(self),
                remote_address=remote_server.listen_client_address,
                remote_port=remote_server.listen_client_port,
                hold=' -H' if self.HOLD_TERM else ''))

        assert (rc == 0)

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

        self.servers.append(server)
        rc = os.system(
            'xfce4-terminal -e "{manage} '
            '--noroot --config {cfgfile} create-pool '
            '--dbname {dbname} '
            '--remote-address {remote_address} '
            '--remote-port {remote_port} '
            '--user {user} '
            '--password {password} '
            '--buffer-path {buffer_path} '
            '--buffer-size {buffer_size}"{hold}'.format(
                manage=MANAGE,
                cfgfile=server.cfgfile,
                user=username,
                password=password,
                **vars(self),
                remote_address=remote_server.listen_client_address,
                remote_port=remote_server.listen_client_port,
                hold=' -H' if self.HOLD_TERM else ''))

        assert (rc == 0)

        if sleep:
            await asyncio.sleep(sleep)


