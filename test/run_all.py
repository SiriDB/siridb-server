#!/usr/bin/python3
from testing import run_test
from test_user import TestUser
from test_insert import TestInsert

if __name__ == '__main__':
    run_test(TestUser())
    run_test(TestInsert())






