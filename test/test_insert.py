import time
from testing import (
    Server,
    Client,
    SiriDB,
    run_test,
    TestBase)

class TestInsert(TestBase):
    title = 'Test inserts and response'

    async def run(self):
        servers = [Server(i) for i in range(3)]
        for server in servers:
            server.create()
            server.start()

        time.sleep(1.0)

        db = SiriDB()
        db.create_on(servers[0])

        time.sleep(1.0)

        client = Client(db, servers[0])
        await client.connect()

        result = await client.query('list users')
        for k, v in result.items():
            print(k, v)

        client.close()

        for server in servers:
            server.stop()


if __name__ == '__main__':
    Server.HOLD_TERM = False
    run_test(TestInsert())
