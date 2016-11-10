'''
Migration webserver:

modify /opt/oversight/webserver/etc/...config json
- remove siridb so webserver creates a new from cloudconstants

git submodule update --init
if required, git checkout master, in shared/siriv1...

supervisorctl restart webserver:

'''
import os
import sys
import time
sys.path.append('../siridb')
from siriclient import SiriClient
sys.path.append('../siridb-connector')
from siridb.connector import connect

RENAME_SERIES = False

def rename_series(series_name):
    return series_name.replace('|', '\u2502').replace('}.{', '|')[1:-1]

def move_series(source, dest, source_is_python):
    result = source.query('count series where type != {}'.format(
        '"string"' if source_is_python else 'string'))
    n = result['series']

    query_time = 0
    insert_time = 0

    while (n):
        n -= 1

        # Select points
        start = time.time()
        result = source.query('list series where type != {} limit 1'.format(
            '"string"' if source_is_python else 'string'))
        series = result['series']
        if not series:
            print('Error listing series: {}'.format(series))
            break
        series_name = series[0][0]
        data = source.query('select * from "{}"'.format(series_name))
        if not series_name in data:
            print('Error getting data: {}'.format(data))
            break
        query_time += (time.time() - start)

        if RENAME_SERIES and series_name.startswith('{perf}'):
            data[rename_series(series_name)] = data.pop(series_name)

        # Insert Data
        start = time.time()
        result = dest.insert(data)
        if not 'success_msg' in result:
            print('Error inserting data: {}'.format(result))
            break
        insert_time += (time.time() - start)

        # Drop old series
        start = time.time()
        result = source.query('drop series "{}" set ignore_threshold true'.format(series_name))
        query_time += (time.time() - start)

    return query_time, insert_time

if __name__ == '__main__':
    # connect to old SiriDB
    siridb_python = SiriClient()
    siridb_python.connect('siri', 'iris', 'sasien', 'localhost', 9100)

    # connect to new SiriDB
    siridb_c = connect('iris', 'siri', 'dbtest', 'localhost', 9000)

    # Uncomment to move series from Python to C
    query_time, insert_time = move_series(
        siridb_python,
        siridb_c,
        source_is_python=True)

    # Uncomment to move series from C to Python
    # query_time, insert_time = move_series(
    #     siridb_c,
    #     siridb_python,
    #     source_is_python=False)


    print('''
Query time  : {}
Insert time : {}
'''.format(query_time, insert_time))

    siridb_python.close()
    siridb_c.close()







