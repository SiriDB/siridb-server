#!/usr/bin/python3
from testing import run_test
from testing import Server
from testing import parse_args
from test_cluster import TestCluster
from test_group import TestGroup
from test_list import TestList
from test_insert import TestInsert
from test_pool import TestPool
from test_select import TestSelect
from test_select_ns import TestSelectNano
from test_series import TestSeries
from test_server import TestServer
from test_user import TestUser
from test_compression import TestCompression
from test_log import TestLog
from test_log import TestLog
from test_pipe_support import TestPipeSupport
from test_buffer import TestBuffer


if __name__ == '__main__':
    parse_args()
    run_test(TestCompression())
    run_test(TestGroup())
    run_test(TestList())
    run_test(TestInsert())
    run_test(TestPool())
    run_test(TestSelect())
    run_test(TestSelectNano())
    run_test(TestSeries())
    run_test(TestServer())
    run_test(TestUser())
    run_test(TestLog())
    run_test(TestPipeSupport())
    run_test(TestBuffer())
