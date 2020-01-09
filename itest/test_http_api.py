import requests
import json

data = {
    'q': 'select * from *'
}

x = requests.post(
    f'http://localhost:9020/query/dbtest',
    data=json.dumps(data),
    auth=('iris', 'siri'),
    headers={'Content-Type': 'application/json'}
)


print(x.content)