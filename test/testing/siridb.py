import os
import logging
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

    def create_on(self, server):
        logging.info('Create database {} on {}'.format(
            self.dbname,
            server.name))

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