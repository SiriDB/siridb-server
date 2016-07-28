import random
import time
from testing import Client
from testing import gen_points
from testing import InsertError
from testing import PoolError
from testing import run_test
from testing import Server
from testing import ServerError
from testing import SiriDB
from testing import TestBase
from testing import UserAuthError

class TestInsert(TestBase):
    title = 'Test inserts and response'

    async def run(self):
        servers = [Server(i) for i in range(1)]
        for server in servers:
            server.create()
            server.start()

        time.sleep(1.0)

        db = SiriDB()
        db.create_on(servers[0])

        time.sleep(1.0)

        client = Client(db, servers[0])
        await client.connect()

        self.assertEqual(
            await client.insert({}),
            {'success_msg': 'Inserted 0 point(s) successfully.'})

        self.assertEqual(
            await client.insert([]),
            {'success_msg': 'Inserted 0 point(s) successfully.'})

        series_float = gen_points(tp=float)
        random.shuffle(series_float)
        series_int = gen_points(tp=int)
        random.shuffle(series_int)

        self.assertEqual(
            await client.insert({
                'series float': series_float,
                'series int': series_int
            }), {'success_msg': 'Inserted 2000 point(s) successfully.'})


        series_float.sort()
        series_int.sort()

        result = await client.query('select * from "series float"')
        self.assertEqual(result['series float'], series_float)

        result = await client.query('select * from "series int"')
        self.assertEqual(result['series int'], series_int)

        self.assertEqual(
            await client.query('list series name, length, type, start, end'),
            {   'columns': ['name', 'length', 'type', 'start', 'end'],
                'series': [
                    ['series float', 1000, 'float', series_float[0][0], series_float[-1][0]],
                    ['series int', 1000, 'integer', series_int[0][0], series_int[-1][0]],
                ]
            })

        with self.assertRaises(InsertError):
            await client.insert('[]')

        with self.assertRaises(InsertError):
            await client.insert('[]')

        with self.assertRaises(InsertError):
            await client.insert([{}])

        with self.assertRaises(InsertError):
            await client.insert({'no points': []})

        with self.assertRaises(InsertError):
            await client.insert({'no points': [[]]})

        with self.assertRaises(InsertError):
            await client.insert({'no points': [[1213]]})

        with self.assertRaises(InsertError):
            await client.insert({'invalid ts': [[0.5, 6]]})

        client.close()

        for server in servers:
            result = await server.stop()
            self.assertTrue(
                result,
                msg='Server {} did not close correctly'.format(server.name))


if __name__ == '__main__':
    SiriDB.HOLD_TERM = False
    Server.HOLD_TERM = False
    run_test(TestInsert())
