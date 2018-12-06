# import re
# import random
# import asyncio
# import functools
# import random
# import time
# import os
# import sys
# from testing import Client
# from testing import default_test_setup
# from testing import gen_data
# from testing import gen_points
# from testing import gen_series
# from testing import InsertError
# from testing import PoolError
# from testing import QueryError
# from testing import run_test
# from testing import Series
# from testing import Server
# from testing import ServerError
# from testing import SiriDB
# from testing import TestBase
# from testing import UserAuthError
# from querybouwer.querybouwer import Querybouwer
# from querybouwer.k_map import k_map
# #from querybouwer.validate import SiriRipOff, compare_result
# from cmath import isfinite
# from math import nan

# from testing.constants import SIRIDBC
# print(SIRIDBC)
# sys.path.append('../grammar/')#os.path.join(SIRIDBC, '/grammar'))
# from pygrammar.grammar import SiriGrammar


# M, N = 20, 70
# #DATA = {str(a).zfill(6): [[b, random.randint(0, 20)] for b in range(N)] for a in range(M)}
# DATA = {str(a).zfill(6): [[b*N+a, random.randint(0, 20)] for b in range(N)] for a in range(M)}
# #DATA = {str(a).zfill(6): [[b, nan if b > 15 else float(random.randint(0, 20))] for b in range(N)] for a in range(M)}

# #client_validate = SiriRipOff()
# #client_validate.groups = {'GROUP': re.compile('.*')}
# #client_validate.update(DATA)


# class TestGrammar(TestBase):
#     title = 'Test select'

#     async def test_main(self):
#         qb = Querybouwer(SiriGrammar, {'regex_map': k_map,
#                 'max_list_n_map': {
#                     'series_match': 1,
#                     'aggregate_functions': 1,
#                     'select_aggregate': 1
#                 },
#                 #'default_list_n': 2,
#                 'replace_map': {
#                     'k_prefix': '',
#                     'k_suffix': '',
#                     'k_where': '',
#                     'after_expr': '',
#                     'before_expr': '',
#                     'between_expr': '',
#                     'k_merge': '',
#                     'r_singleq_str': '',

#                     #'f_median': '',
#                     #'f_median_low': '',
#                     #'f_median_high': '',
#                     'k_now': '',
#                     'r_float': '',
#                     'r_time_str': '',
#                     #'r_grave_str': '',
#                     #'f_filter': '',
#                     }
#                 })

#         for q in qb.generate_queries('select_stmt'):
#             #if '~' in q: continue
#             #if 'between now' in q: continue
#             #if "000000" in q or '1970' in q or '*' in q or '==' in q or '>=' in q or '<' in q: continue

#             #if 'filter' in q and 'derivative' in q.split('filter', 1)[0]: continue
#             q = ' suffix "a", '.join(q.split('   , ', 1))
#             #a, b = q.split('   , ', 1)
#             #q = a + ' suffix "b",'+b

#                         #q = 'select * from /.*/ '+q
#             try:
#                 res0 = await self.client0.query(q)
#             except:
#                 pass
#             #res1 = client_validate.query(q)
#             #self.assertFalse(compare_result(res0, res1))

#     async def test_where(self):
#         qb = Querybouwer(SiriGrammar, {'regex_map': k_map,
#             'max_list_n_map': {
#                 'series_match': 1,
#                 'aggregate_functions': 1},
#             'replace_map': {
#                 'k_prefix': '',
#                 'k_suffix': 'suffix',
#                 #'k_where': '',
#                 'after_expr': '',
#                 'before_expr': '',
#                 'between_expr': '',
#                 'k_merge': '',
#                 'r_singleq_str': '',
#                 }
#             })
#         for q in qb.generate_queries('select_stmt'):
#             if '~' in q: continue
#             if 'between now' in q: continue
#             print(q)
#             res0 = await self.client0.query(q)
#             #res1 = client_validate.query(q)
#             #self.assertFalse(compare_result(res0, res1))

#     async def test_between(self):
#         qb = Querybouwer(SiriGrammar, {'regex_map': k_map,
#             'max_list_n_map': {
#                 'series_match': 1,
#                 'aggregate_functions': 1},
#             'replace_map': {
#                 'k_prefix': '',
#                 'k_suffix': '',
#                 'k_where': '',
#                 'after_expr': '',
#                 'before_expr': '',
#                 #'between_expr': '',
#                 'k_merge': '',
#                 'r_singleq_str': '',
#                 }
#             })
#         for q in qb.generate_queries('select_stmt'):
#             if '~' in q: continue
#             if 'between now' in q: continue
#             print(q)
#             res = await self.client0.query(q)
#             #self.assertFalse(validate_query_result(q, res))

#     async def test_merge(self):
#         qb = Querybouwer(SiriGrammar, {'regex_map': k_map,
#             'max_list_n_map': {
#                 'series_match': 1,
#                 'aggregate_functions': 1},
#             'replace_map': {
#                 'k_prefix': '',
#                 'k_suffix': '',
#                 'k_where': '',
#                 'after_expr': '',
#                 'before_expr': '',
#                 'between_expr': '',
#                 #'k_merge': '',
#                 'r_singleq_str': '',
#                 }
#             })
#         for q in qb.generate_queries('select_stmt'):
#             if '~' in q: continue
#             if 'between now' in q: continue
#             print(q)
#             res = await self.client0.query(q)


#     @default_test_setup(1)
#     async def run(self):
#         await self.client0.connect()

#         for k, v in sorted(DATA.items()):
#             await self.client0.insert({k: v})
#         await self.client0.query('create group `GROUP` for /.*/')
#         #time.sleep(2)

#         await self.test_main()
#         #await self.test_where()
#         #await self.test_between()
#         #await self.test_merge()

#         self.client0.close()

#         return False


# if __name__ == '__main__':
#     SiriDB.LOG_LEVEL = 'CRITICAL'
#     Server.HOLD_TERM = True
#     Server.MEM_CHECK = True
#     Server.BUILDTYPE = 'Debug'
#     run_test(TestAll())
#     #run_test(TestSelect())
