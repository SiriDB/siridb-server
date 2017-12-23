import os
import logging
import configparser
import subprocess
import psutil
import asyncio
import time
import socket
import platform
from .constants import SIRIDBC
from .constants import TEST_DIR
from .constants import VALGRIND
from .constants import BUILDTYPE
from .constants import MAX_OPEN_FILES

MEM_PROC = \
    'memcheck-amd64-' if platform.architecture()[0] == '64bit' else \
    'memcheck-x86-li'


class Server:
    HOLD_TERM = False
    GEOMETRY = '140x60'
    MEM_CHECK = False
    BUILDTYPE = BUILDTYPE
    SERVER_ADDRESS = '%HOSTNAME'
    IP_SUPPORT = 'ALL'
    USE_XFCE4 = False
    BIND_CLIENT_ADDRESS = "::"
    BIND_SERVER_ADDRESS = "::"

    def __init__(self,
                 n,
                 optimize_interval=30,
                 heartbeat_interval=30,
                 compression=True,
                 **unused):
        self.n = n
        self.compression = compression
        self.listen_client_port = 9000 + n
        self.listen_backend_port = 9010 + n
        self._server_address = self.SERVER_ADDRESS
        self.server_address = \
            self._server_address.lstrip('[').rstrip(']').replace(
                '%HOSTNAME', socket.gethostname())
        self.ip_support = self.IP_SUPPORT
        self.optimize_interval = optimize_interval
        self.heartbeat_interval = heartbeat_interval
        self.bind_client_address = self.BIND_CLIENT_ADDRESS
        self.bind_server_address = self.BIND_SERVER_ADDRESS
        self.cfgfile = os.path.join(TEST_DIR, 'siridb{}.conf'.format(self.n))
        self.dbpath = os.path.join(TEST_DIR, 'dbpath{}'.format(self.n))
        self.name = 'SiriDB:{}'.format(self.listen_backend_port)
        self.pid = None

    @property
    def addr(self):
        return '{}:{}'.format(self.server_address, self.listen_client_port)

    def create(self):
        logging.info('Create server {}'.format(self.name))

        config = configparser.RawConfigParser()
        config.add_section('siridb')
        config.set('siridb', 'listen_client_port', self.listen_client_port)
        config.set('siridb', 'bind_client_address', self.bind_client_address)
        config.set('siridb', 'bind_server_address', self.bind_server_address)
        config.set('siridb', 'server_name', '{}:{}'.format(
            self._server_address,
            self.listen_backend_port))
        config.set('siridb', 'ip_support', self.ip_support)
        config.set('siridb', 'optimize_interval', self.optimize_interval)
        config.set('siridb', 'heartbeat_interval', self.heartbeat_interval)
        config.set('siridb', 'default_db_path', self.dbpath)
        config.set('siridb', 'max_open_files', MAX_OPEN_FILES)
        config.set('siridb', 'enable_shard_compression', int(self.compression))

        with open(self.cfgfile, 'w') as configfile:
            config.write(configfile)

        try:
            os.mkdir(self.dbpath)
        except FileExistsError:
            pass

    def _get_pid_set(self):
        try:
            ret = set(map(int, subprocess.check_output([
                'pgrep',
                MEM_PROC if self.MEM_CHECK else 'siridb-server']).split()))
        except subprocess.CalledProcessError:
            ret = set()
        return ret

    async def start(self, sleep=None):
        prev = self._get_pid_set()

        if self.USE_XFCE4:
            rc = subprocess.Popen(
                'xfce4-terminal -e "{}{} --config {} --log-colorized"'
                ' --title {} --geometry={}{}'
                .format(VALGRIND if self.MEM_CHECK else '',
                        SIRIDBC.format(BUILDTYPE=self.BUILDTYPE),
                        self.cfgfile,
                        self.name,
                        self.GEOMETRY,
                        ' -H' if self.HOLD_TERM else ''),
                shell=True)
        else:
            rc = subprocess.Popen(
               'xterm {}-title {} -geometry {} -e "{}{} --config {}"'
               .format('-hold ' if self.HOLD_TERM else '',
                       self.name,
                       self.GEOMETRY,
                       VALGRIND if self.MEM_CHECK else '',
                       SIRIDBC.format(BUILDTYPE=self.BUILDTYPE),
                       self.cfgfile),
               shell=True)

        await asyncio.sleep(1)

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
        print("!!!!!!!!!!!! KILLL !!!!!!!!!!")
        os.system('kill -9 {}'.format(self.pid))
        self.pid = None

    def is_active(self):
        return False if self.pid is None else psutil.pid_exists(self.pid)
