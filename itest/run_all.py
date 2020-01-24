#!/usr/bin/python3
from testing import run_test
from testing import Server
from testing import parse_args
from test_buffer import TestBuffer
from test_cluster import TestCluster
from test_compression import TestCompression
from test_create_database import TestCreateDatabase
from test_expiration import TestExpiration
from test_group import TestGroup
from test_http_api import TestHTTPAPI
from test_insert import TestInsert
from test_list import TestList
from test_log import TestLog
from test_log import TestLog
from test_parentheses import TestParenth
from test_pipe_support import TestPipeSupport
from test_pool import TestPool
from test_select import TestSelect
from test_select_ns import TestSelectNano
from test_series import TestSeries
from test_server import TestServer
from test_tee import TestTee
from test_user import TestUser


if __name__ == '__main__':
    parse_args()
    run_test(TestBuffer())
    run_test(TestCompression())
    run_test(TestCreateDatabase())
    run_test(TestExpiration())
    run_test(TestGroup())
    run_test(TestHTTPAPI())
    run_test(TestInsert())
    run_test(TestList())
    run_test(TestLog())
    run_test(TestParenth())
    run_test(TestPipeSupport())
    run_test(TestPool())
    run_test(TestSelect())
    run_test(TestSelectNano())
    run_test(TestSeries())
    run_test(TestServer())
    run_test(TestTee())
    run_test(TestUser())
