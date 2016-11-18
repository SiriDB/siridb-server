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
# sys.path.append('../../siridb')
sys.path.append('SiriDB')
from siriclient import SiriClient
from siridb.connector import connect


def copy_series(source, dest, source_is_python):
    result = source.query('count series')
    n = result['series']
    if n > 10000:
        raise ValueError('Too many series to copy')

    result = source.query('list series limit 10000')
    series_list = result['series']

    if len(series_list) != n:
        raise IndexError('List series does not match count series')

    query_time = 0
    insert_time = 0

    for series in series_list:
        # Select points
        start = time.time()
        series_name = series[0]
        print('Copy series: {}'.format(series_name))
        data = source.query('select * from "{}"'.format(series_name))
        query_time += (time.time() - start)

        # Insert Data
        start = time.time()
        result = dest.insert(data)
        if not 'success_msg' in result:
            print('Error inserting data: {}'.format(result))
            break
        insert_time += (time.time() - start)


    return query_time, insert_time

if __name__ == '__main__':
    # connect to source SiriDB
    siridb_src = SiriClient()
    siridb_src.connect('joente', '', 'oversight', 'siri01.c.detect-analyze-notify-01a.internal', 9000)

    # connect to destination SiriDB
    siridb_dst = SiriClient()
    siridb_dst = connect('joente', '', 'oversight', 'sirilog01', 9000)

    # Uncomment to copy series from source to dest
    try:
        query_time, insert_time = copy_series(
            siridb_src,
            siridb_dst,
            source_is_python=True)
    except Exception as e:
        print('Got an exception: {}'.format(e))
    else:
        print('''
Query time  : {}
Insert time : {}
'''.format(query_time, insert_time))
    finally:
        siridb_src.close()
        siridb_dst.close()







