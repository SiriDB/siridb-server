#!/usr/bin/python3
from testing import run_test
from testing import Server
from test_cluster import TestCluster
from test_group import TestGroup
from test_insert import TestInsert
from test_pool import TestPool
from test_select import TestSelect
from test_series import TestSeries
from test_server import TestServer
from test_user import TestUser

Server.BUILDTYPE = 'Release'

if __name__ == '__main__':
    # run_test(TestCluster())
    run_test(TestGroup())
    run_test(TestInsert())
    run_test(TestPool())
    run_test(TestSelect())
    run_test(TestSeries())
    run_test(TestServer())
    run_test(TestUser())





