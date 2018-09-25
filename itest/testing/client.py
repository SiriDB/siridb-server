import sys
import asyncio
import functools
import logging
import random
import time
from siridb.connector import SiriDBClient
from siridb.connector.lib.exceptions import AuthenticationError
from siridb.connector.lib.exceptions import InsertError
from siridb.connector.lib.exceptions import PoolError
from siridb.connector.lib.exceptions import QueryError
from siridb.connector.lib.exceptions import ServerError
from siridb.connector.lib.exceptions import UserAuthError
from .helpers import gen_points
from .server import Server


class Client:
    def __init__(self, db, servers, username='iris', password='siri'):
        self.db = db

        if isinstance(servers, Server):
            servers = [servers]

        self.cluster = SiriDBClient(
            username=username,
            password=password,
            dbname=db.dbname,
            hostlist=[
                (server.server_address, server.listen_client_port)
                for server in servers
            ])

        self.query = self.cluster.query
        self.insert = self.cluster.insert

    async def connect(self):
        logging.info('Create client connection to database {}'.format(
            self.db.dbname))
        await self.cluster.connect()

    def close(self):
        logging.info('Closing connection to database {}'.format(
            self.db.dbname))
        self.cluster.close()

    async def insert_some_series(self,
                                 series,
                                 n=0.01,
                                 timeout=None,
                                 points=functools.partial(gen_points, n=1)):
        random.shuffle(series)

        n = int(len(series) * n)

        assert (n <= len(series) and n > 0)

        data = {s.name: s.add_points(points()) for s in series[:n]}

        while True:
            try:
                await self.insert(data)
            except PoolError as e:
                if not timeout:
                    raise e
                timeout -= 1
                await asyncio.sleep(1.0)
            else:
                break

        if timeout is not None:
            time.sleep(0.1)

        for s in series[:n]:
            s.commit_points()
