import requests
import json
from testing import gen_points

TIME_PRECISION = 's'

data = {
    'q': 'select * from "aggr"',
}

x = requests.post(
    f'http://localhost:9020/query/dbtest',
    data=json.dumps(data),
    auth=('iris', 'siri'),
    headers={'Content-Type': 'application/json'}
)

print(x.status_code)

if x.status_code == 200:
    print(x.json())
else:
    print(x.text)

series_float = gen_points(
    tp=float, n=10000, time_precision=TIME_PRECISION, ts_gap='5m')

series_int = gen_points(
    tp=int, n=10000, time_precision=TIME_PRECISION, ts_gap='5m')

data = {
    'my_float': series_float,
    'my_int': series_int
}

x = requests.post(
    f'http://localhost:9020/insert/dbtest',
    data=json.dumps(data),
    auth=('iris', 'siri'),
    headers={'Content-Type': 'application/json'}
)

print(x.status_code)
if x.status_code == 200:
    print(x.json())
else:
    print(x.text)
