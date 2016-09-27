import os
import logging
import configparser
import subprocess
import psutil
import asyncio
import time
from .constants import SIRIDBC
from .constants import TEST_DIR
from .constants import VALGRIND

class Server:
    HOLD_TERM = False
    GEOMETRY = '140x60'
    MEM_CHECK = False

    def __init__(self,
                 n,
                 optimize_interval=30,
                 heartbeat_interval=30):
        self.n = n
        self.listen_client_address = 'localhost'
        self.listen_client_port = 9000 + n
        self.listen_backend_address = 'localhost'
        self.listen_backend_port = 9010 + n
        self.optimize_interval = optimize_interval
        self.heartbeat_interval = heartbeat_interval
        self.cfgfile = os.path.join(TEST_DIR, 'siridb{}.conf'.format(self.n))
        self.dbpath = os.path.join(TEST_DIR, 'dbpath{}'.format(self.n))
        self.name = 'SiriDB:{}'.format(self.listen_backend_port)
        self.pid = None

    def create(self):
        logging.info('Create server {}'.format(self.name))

        config = configparser.RawConfigParser()
        config.add_section('siridb')
        config.set('siridb', 'listen_client', '{}:{}'.format(
            self.listen_client_address,
            self.listen_client_port))
        config.set('siridb', 'listen_backend', '{}:{}'.format(
            self.listen_backend_address,
            self.listen_backend_port))
        config.set('siridb', 'optimize_interval', self.optimize_interval)
        config.set('siridb', 'heartbeat_interval', self.heartbeat_interval)
        config.set('siridb', 'default_db_path', self.dbpath)

        with open(self.cfgfile, 'w') as configfile:
            config.write(configfile)

        try:
            os.mkdir(self.dbpath)
        except FileExistsError:
            pass

    def _get_pid_set(self):
        # print(subprocess.check_output(['pgrep', 'memcheck-amd64-']))
        try:

            ret = set(map(int, subprocess.check_output(['pgrep', 'memcheck-amd64-' if self.MEM_CHECK else 'siridbc']).split()))
        except subprocess.CalledProcessError:
            ret = set()
        return ret

    async def start(self, sleep=None):
        prev = self._get_pid_set()
        rc = os.system(
            'xfce4-terminal -e "{}{} --config {}" --title {} --geometry={}{}'
            .format(VALGRIND if self.MEM_CHECK else '',
                    SIRIDBC,
                    self.cfgfile,
                    self.name,
                    self.GEOMETRY,
                    ' -H' if self.HOLD_TERM else ''))

        if self.MEM_CHECK:
            time.sleep(5)

        my_pid = self._get_pid_set() - prev
        assert (len(my_pid) == 1)
        self.pid = my_pid.pop()
        if sleep:
            await asyncio.sleep(sleep)

    async def stop(self, timeout=20):
        if self.is_active():
            os.system('kill {}'.format(self.pid))

            while (timeout and self.is_active()):
                await asyncio.sleep(1.0)
                timeout -= 1

        if timeout:
            self.pid = None
            return True

        return False

    async def stopstop(self):
        if self.is_active():
            os.system('kill {}'.format(self.pid))
            await asyncio.sleep(0.2)

            if self.is_active():
                os.system('kill {}'.format(self.pid))
                await asyncio.sleep(1.0)

                if self.is_active():
                    return False

        self.pid = None
        return True

    def kill(self):
        os.system('kill -9 {}'.format(self.pid))
        self.pid = None

    def is_active(self):
        return False if self.pid is None else psutil.pid_exists(self.pid)
