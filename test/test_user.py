import time
from testing import (
    Server,
    Client,
    SiriDB,
    run_test,
    TestBase)

class TestUser(TestBase):
    title = 'Test user object'

    async def run(self):
        servers = [Server(i) for i in range(3)]
        for server in servers:
            server.create()
            server.start()

        time.sleep(1.0)

        db = SiriDB()
        db.create_on(servers[0])

        time.sleep(1.0)

        client0 = Client(db, servers[0])
        await client0.connect()

        result = await client0.query('list users')
        assert result.pop('users') == [['iris', 'full']]

        result = await client0.query('create user "sasientje" set password "blabla"')
        assert result.pop('success_msg') == "User 'sasientje' is created successfully."

        result = await client0.query('list users where access < modify')
        assert result.pop('users') == [['sasientje', 'no access']]

        result = await client0.query('grant modify to user "sasientje"')
        assert result.pop('success_msg') == "Successfully granted permissions to user 'sasientje'."

        client0.close()

        for server in servers:
            result = await server.stop()
            assert result == True, 'Server {} did not close'.format(server.name)


if __name__ == '__main__':
    Server.HOLD_TERM = False
    run_test(TestUser())
