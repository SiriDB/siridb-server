# import sys
# from _collections import defaultdict
# from statistics import mean, median, median_low, median_high, variance, pvariance
# from datetime import datetime
# from numpy import nan, inf, isnan, isfinite
# import time
# import re
# sys.path.append('../grammar')
# from pygrammar.grammar import SiriGrammar  # nopep8
# # import decimal
# # getcontext().prec = 12
# PRECISION = 9

# expr_map = {
#     '==': lambda a, b: a==b,
#     '!=': lambda a, b: a!=b,
#     '<=': lambda a, b: a<=b,
#     '>=': lambda a, b: a>=b,
#     '<': lambda a, b: a<b,
#     '>': lambda a, b: a>b,
#     '!~': lambda a, b: True,
#     '~': lambda a, b: True,
# }
# prop_map = {
#     'length': lambda k, v: len(v),
#     'pool': lambda k, v: False,
#     'name': lambda k, v: k,
#     'start': lambda k, v: v[0][0],
#     'end': lambda k, v: v[-1][0],
#     'type': lambda k, v: 'integer' if type(v[0][1])==int else 'float',
# }
# setop_map = {
#     'k_union': lambda a, b: a | b,
#     'c_difference': lambda a, b: a - b,
#     'k_symmetric_difference': lambda a, b: a ^ b,
#     'k_intersection': lambda a, b: a & b,
# }

# seconds_map = {'s': 1, 'm': 60, 'h': 3600, 'd': 86400, 'w': 604800}


# def difference(pts, ts):
#     if ts:
#         return [[t, 0 if len(v)==1 else v[-1]-v[0]] for t, v in sorted(binfun(pts, ts[0]))]#KLOPT DIT? (nan-nan) 0 if len(v)==1 else
#     else:
#         return [[t, v1-v0] for (_, v0), (t, v1) in zip(pts[:-1], pts[1:])]


# def derivative(pts, ts):
#     ts = (ts[0] if ts else 1)
#     return [[t1, inf if (t0==t1 and (v1-v0)>0) else \
#                  -inf if (t0==t1 and (v1-v0)<0) else \
#                  nan if t0==t1 else \
#                  ts*(v1-v0)/(t1-t0)#round(ts*(v1-v0)/(t1-t0), PRECISION)
#         ] for (t0, v0), (t1, v1) in zip(pts[:-1], pts[1:])]


# def pvariance_(vs):
#     nans = [a for a in vs if not isfinite(a)]
#     return nan if nans else 0 if len(vs) == 1 else pvariance(vs)


# def binfun(pts, size):
#     bins = defaultdict(list)
#     for t, v in pts:
#         bins[(t-1)//size*size+size].append(v)
#     return bins.items()


# def binfunmerge(series, size):
#     bins = defaultdict(list)
#     for _, pts in sorted(series.items(), reverse=True):
#         for t, v in pts:
#             bins[(t-1)//size*size+size].append(v)
#     return bins.items()


# def mergefun(series):
#     return sorted([tv for _, pts in sorted(series.items(), reverse=True) for tv in pts], key=lambda a: a[0])

# funmap = {
#     'f_points': lambda pts, ts: pts,
#     'f_sum': lambda pts, ts: sorted([t, sum(v)] for t, v in binfun(pts, ts[0])),
#     'f_mean': lambda pts, ts: sorted([t, mean(v)] for t, v in binfun(pts, ts[0])),
#     'f_median': lambda pts, ts: sorted([t, median(v)] for t, v in binfun(pts, ts[0])),
#     'f_median_low': lambda pts, ts: sorted([t, median_low(v)] for t, v in binfun(pts, ts[0])),
#     'f_median_high': lambda pts, ts: sorted([t, median_high(v)] for t, v in binfun(pts, ts[0])),
#     'f_min': lambda pts, ts: sorted([t, min(v)] for t, v in binfun(pts, ts[0])),
#     'f_max': lambda pts, ts: sorted([t, max(v)] for t, v in binfun(pts, ts[0])),
#     'f_count': lambda pts, ts: sorted([t, len(v)] for t, v in binfun(pts, ts[0])),
#     'f_variance': lambda pts, ts: sorted([t, 0 if len(v) ==1 else variance(v)] for t, v in binfun(pts, ts[0])),
#     'f_pvariance': lambda pts, ts: sorted([t, pvariance_(v)] for t, v in binfun(pts, ts[0])),
#     'f_difference': difference,
#     'f_derivative': derivative,
#     'f_filter': lambda pts, ts: [tv for tv in pts if ts[0](tv[1], ts[1])],#round(tv[1], 6)
# }

# funmapmerge = {
#     'f_points': lambda series, ts: mergefun(series),
#     'f_sum': lambda series, ts: sorted([t, sum(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_mean': lambda series, ts: sorted([t, mean(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_median': lambda series, ts: sorted([t, median(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_median_low': lambda series, ts: sorted([t, median_low(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_median_high': lambda series, ts: sorted([t, median_high(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_min': lambda series, ts: sorted([t, min(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_max': lambda series, ts: sorted([t, max(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_count': lambda series, ts: sorted([t, len(v)] for t, v in binfunmerge(series, ts[0])),
#     'f_variance': lambda series, ts: sorted([t, round(variance(v), PRECISION) if len(v) > 1 else 0] for t, v in binfunmerge(series, ts[0])),
#     'f_pvariance': lambda series, ts: sorted([t, round(pvariance(v), PRECISION) if len(v) > 1 else 0] for t, v in binfunmerge(series, ts[0])),
#     'f_difference': lambda series, ts: difference(mergefun(series), ts),
#     'f_derivative': lambda series, ts: derivative(mergefun(series), ts),
#     'f_filter': lambda series, ts: [tv for tv in mergefun(series) if ts[0](tv[1], ts[1])],
# }


# class SiriRipOff(dict):
#     groups = {}

#     def _time_expr(self, e):
#         for e1 in e.children:
#             if e1.children[0].element.name == 'r_integer':
#                 return int(e1.string)
#             elif e1.children[0].element.name == 'r_time_str':
#                 return int(e1.string[:-1])*seconds_map[e1.string[-1:]]
#             elif e1.children[0].element.name == 'k_now':
#                 return int(time.time())
#             else:
#                 alist = e1.string[1:-1].split()
#                 Y, M, D = map(int, alist[0].split('-'))
#                 if len(alist) == 2:
#                     h, m, s = map(int, (alist[1]+':0:0').split(':')[:3])
#                     dt = datetime(Y, M, D, h, m, s)
#                 else:
#                     dt = datetime(Y, M, D)
#                 return int(time.mktime(dt.timetuple()))

#     def _aggrfun(self, e):
#         ename = getattr(e.element, 'name', None)
#         texprs = []
#         if ename == 'f_filter':
#             oname = e.children[2].children[0].element.name
#             op = expr_map[e.children[2].string] if oname == 'str_operator' else expr_map['==']
#             for e1 in e.children[-2].children:
#                 if getattr(e1.element, 'name', None) == 'r_integer':
#                     val = int(e1.string)
#                 elif getattr(e1.element, 'name', None) == 'r_float':
#                     val = float(e1.string)
#                 else:
#                     assert 0
#             texprs = [op, val]
#         else:
#             for e1 in e.children:
#                 if getattr(e1.element, 'name', None) == 'time_expr':
#                     texprs.append(self._time_expr(e1))
#                 else:
#                     for e2 in e1.children:
#                         if getattr(e2.element, 'name', None) == 'time_expr':
#                             texprs.append(self._time_expr(e2))

#         return ename, texprs

#     def _from(self, e):
#         seriessets = []
#         operators = []
#         for e1 in [a.children[0] for a in e.children]:
#             ename = getattr(e1.element, 'name', None)
#             if ename == 'series_name':
#                 seriessets.append({e1.string[1:-1]} if e1.string[1:-1] in self else set())
#             elif ename == 'series_re':
#                 r = re.compile(e1.string[1:-1])
#                 seriessets.append({name for name in self if r.match(name)})
#             elif ename == 'group_match':
#                 r = self.groups[e1.string[1:-1]]
#                 seriessets.append({name for name in self if r.match(name)})
#             else:
#                 operators.append(setop_map[ename])

#         _series = seriessets[0]
#         for s, o in zip(seriessets[1:], operators):
#             _series = o(_series, s)
#         return _series

#     def _where(self, e):
#         wfun = []
#         for e1 in e.children[0].children[1].children[0].children:
#             prop, exp, val = e1.children
#             vtyp = getattr(val.element, 'name', None)
#             if vtyp == 'string':
#                 val = val.string[1:-1]
#             elif vtyp == 'int_expr':
#                 val = int(val.string)
#             elif vtyp == 'time_expr':
#                 val = self._time_expr(val)
#             else:
#                 val = val.string

#             wfun.append(lambda k, v: expr_map[exp.string](prop_map[prop.string](k, v), val))

#         return lambda k, v: sum(fun(k, v) for fun in wfun)==len(wfun)

#     def _aggr(self, e):
#         aggr = []
#         for i, e1 in enumerate(e.children):
#             if i%2==1: continue
#             _prefix = ''
#             _suffix = ''
#             _aggrfuns = []
#             for i, e2 in enumerate(e1.children[0].children):
#                 if i%2==1: continue
#                 fun, ts = self._aggrfun(e2.children[0])
#                 _aggrfuns.append([fun, ts])

#             for e2 in e1.children[1:]:
#                 if e2.children[0].element.name == 'prefix_expr':
#                     _prefix = e2.children[0].children[1].string[1:-1]
#                 elif e1.children[0].element.name == 'suffix_expr':
#                     _suffix = e2.children[0].children[1].string[1:-1]
#             aggr.append([_aggrfuns, _prefix, _suffix])
#         return aggr

#     def _merge(self, e):
#         mergeas = e.children[0].children[2].string[1:-1]
#         _aggrfuns = []
#         if e.children[0].children[-1].children[0].children:
#             for i, e1 in enumerate(e.children[0].children[-1].children[0].children[1].children):
#                 if i%2==1: continue
#                 fun, ts = self._aggrfun(e1.children[0])
#                 _aggrfuns.append([fun, ts])
#         return mergeas, _aggrfuns or [['f_points', None]]


#     def select_stmt(self, e):
#         aggr = self._aggr(e.children[1])
#         names = self._from(e.children[3])
#         wfun, merge, after, before = None, None, None, None
#         for e1 in e.children[4:]:
#             lname = getattr(e1.children[0].children[0].element, 'name', None)
#             if lname == 'k_where':
#                 wfun = self._where(e1)
#             elif lname == 'k_merge':
#                 merge = self._merge(e1)
#             elif lname == 'after_expr':
#                 after = self._time_expr(e1.children[0].children[0].children[1])
#             elif lname == 'before_expr':
#                 before = self._time_expr(e1.children[0].children[0].children[1])
#             elif lname == 'between_expr':
#                 after = self._time_expr(e1.children[0].children[0].children[1])
#                 before = self._time_expr(e1.children[0].children[0].children[3])

#         series = {}
#         for name in names:
#             pts = self[name]
#             if wfun and not wfun(name, pts): continue
#             if after is not None and before:
#                 pts = [[t, v] for t, v in pts if after <= t < before]
#             elif after is not None:
#                 pts = [[t, v] for t, v in pts if after <= t]
#             elif before is not None:
#                 pts = [[t, v] for t, v in pts if t < before]

#             for aggrs, prefix, suffix in aggr:
#                 for funname, ts in aggrs:
#                     pts = funmap[funname](pts, ts)
#                 series[prefix+name+suffix] = pts

#         lname = getattr(e.children[-1].children[0].element, 'name', None)
#         if merge:
#             mergeas, aggrs = merge
#             for funname, ts in aggrs[:1]:
#                 pts = funmapmerge[funname](series, ts)
#             for funname, ts in aggrs[1:]:
#                 pts = funmap[funname](pts, ts)

#             return {mergeas: pts}

#         return series

#     def query(self, q):
#         res = siri_grammar.parse(q)
#         for e in res.tree.children[0].children[0].children[0].children:
#             ename = getattr(e.element, 'name', None)
#             return getattr(self, ename)(e)


# def compare_result(res0, res1):
#     if res0.keys()!=res1.keys():
#         return True

#     for k, v in res0.items():
#         v_val = res1[k]
#         if v == v_val: continue
#         n_ok = sum(v0==v1 or round(v0-v1, 3)==0 or (isnan(v0) and isnan(v1)) for (t0, v0), (t1, v1) in zip(v, v_val))
#         if n_ok == len(v) == len(v_val): continue
#         print(n_ok)
#         print('s', len(v), v)
#         print('k', len(v_val), v_val)
#         print(len(v), len(v_val))
#         return True
#     return False