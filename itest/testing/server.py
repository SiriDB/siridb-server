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
from .constants import MAX_OPEN_FILES
from .color import Color


MEM_PROC = \
    'memcheck-amd64-' if platform.architecture()[0] == '64bit' else \
    'memcheck-x86-li'


def get_file_content(fn):
    with open(fn, 'r') as f:
        data = f.read()
        return data


class Server:
    HOLD_TERM = False
    GEOMETRY = '140x60'
    MEM_CHECK = False
    BUILDTYPE = 'Release'
    SERVER_ADDRESS = '%HOSTNAME'
    IP_SUPPORT = 'ALL'
    TERMINAL = None  # one of [ 'XTERM', 'XFCE4_TERMINAL', None  ]
    BIND_CLIENT_ADDRESS = "::"
    BIND_SERVER_ADDRESS = "::"

    def __init__(self,
                 n,
                 title,
                 optimize_interval=300,
                 heartbeat_interval=30,
                 buffer_sync_interval=500,
                 compression=True,
                 pipe_name=None,
                 **unused):
        self.n = n
        self.test_title = title.lower().replace(' ', '_')
        self.compression = compression
        self.enable_pipe_support = int(bool(pipe_name))
        self.pipe_name = \
            'siridb_client.sock' if not self.enable_pipe_support else \
            pipe_name
        self.listen_client_port = 9000 + n
        self.listen_backend_port = 9010 + n
        self.buffer_sync_interval = buffer_sync_interval
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
        self.buffer_path = None
        self.buffer_size = None

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
        config.set('siridb', 'buffer_sync_interval', self.buffer_sync_interval)
        config.set('siridb', 'default_db_path', self.dbpath)
        config.set('siridb', 'max_open_files', MAX_OPEN_FILES)
        config.set('siridb', 'enable_shard_compression', int(self.compression))
        config.set('siridb', 'enable_pipe_support', self.enable_pipe_support)
        config.set('siridb', 'pipe_client_name',  self.pipe_name)

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
        if self.TERMINAL == 'xfce4-terminal':
            self.proc = subprocess.Popen(
                'xfce4-terminal -e "{}{} --config {} --log-colorized"'
                ' --title {} --geometry={}{}'
                .format(VALGRIND if self.MEM_CHECK else '',
                        SIRIDBC.format(BUILDTYPE=self.BUILDTYPE),
                        self.cfgfile,
                        self.name,
                        self.GEOMETRY,
                        ' -H' if self.HOLD_TERM else ''),
                shell=True)
        elif self.TERMINAL == 'xterm':
            self.proc = subprocess.Popen(
               'xterm {}-title {} -geometry {} -e "{}{} --config {}"'
               .format('-hold ' if self.HOLD_TERM else '',
                       self.name,
                       self.GEOMETRY,
                       VALGRIND if self.MEM_CHECK else '',
                       SIRIDBC.format(BUILDTYPE=self.BUILDTYPE),
                       self.cfgfile),
               shell=True)
        elif self.TERMINAL is None:
            errfn = f'testdir/{self.test_title}-{self.name}-err.log'
            outfn = f'testdir/{self.test_title}-{self.name}-out.log'
            with open(errfn, 'a') as err:
                with open(outfn, 'a') as out:
                    self.proc = subprocess.Popen(
                        '{}{} --config {}'
                        .format(VALGRIND if self.MEM_CHECK else '',
                                SIRIDBC.format(BUILDTYPE=self.BUILDTYPE),
                                self.cfgfile),
                        stderr=err,
                        stdout=out,
                        shell=True)

        await asyncio.sleep(5)

        my_pid = self._get_pid_set() - prev
        if len(my_pid) != 1:
            if self.TERMINAL is None:
                if os.path.exists(outfn) and os.path.getsize(outfn):
                    reasoninfo = get_file_content(outfn)
                elif os.path.exists(errfn):
                    reasoninfo = get_file_content(errfn)
                else:
                    reasoninfo = 'unknown'
                assert 0, (
                    f'{Color.error("Failed to start SiriDB server")}\n'
                    f'{Color.info(reasoninfo)}\n')
            else:
                assert 0, (
                    'Failed to start SiriDB server. A possible reason could '
                    'be that another process is using the same port.')

        self.pid = my_pid.pop()
        if sleep:
            await asyncio.sleep(sleep)

    async def stop(self, timeout=20):
        if self.is_active():
            os.system('kill {}'.format(self.pid))

            while (timeout and self.is_active()):
                await asyncio.sleep(1.0)
                timeout -= 1

        self.proc.communicate()
        assert (self.proc.returncode == 0)

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

    def set_buffer_size(self, db, buffer_size):
        self.buffer_size = buffer_size
        config = configparser.RawConfigParser()
        config.add_section('buffer')
        if self.buffer_path is not None:
            config.set('buffer', 'path', self.buffer_path)
        config.set('buffer', 'size', self.buffer_size)
        with open(os.path.join(
                self.dbpath, db.dbname, 'database.conf'), 'w') as f:
            config.write(f)

    def set_buffer_path(self, db, buffer_path):
        assert(buffer_path.endswith('/'))
        curfile = os.path.join(self.dbpath, db.dbname, 'buffer.dat') \
            if self.buffer_path is None else \
            os.path.join(self.buffer_path, 'buffer.dat')
        if not os.path.exists(buffer_path):
            os.makedirs(buffer_path)
        os.rename(curfile, os.path.join(buffer_path, 'buffer.dat'))
        self.buffer_path = buffer_path
        config = configparser.RawConfigParser()
        config.add_section('buffer')
        config.set('buffer', 'path', self.buffer_path)
        if self.buffer_size is not None:
            config.set('buffer', 'size', self.buffer_size)
        with open(os.path.join(
                self.dbpath, db.dbname, 'database.conf'), 'w') as f:
            config.write(f)

    def kill(self):
        print("!!!!!!!!!!!! KILLL !!!!!!!!!!")
        os.system('kill -9 {}'.format(self.pid))
        self.pid = None

    def is_active(self):
        return False if self.pid is None else psutil.pid_exists(self.pid)
