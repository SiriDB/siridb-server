import sys
import logging
import functools
import random
from .constants import PY_SIRIDB_PATH
from .server import Server
from .helpers import gen_points

sys.path.append(PY_SIRIDB_PATH)
from siriclient import AsyncSiriCluster
from siriclient.shared.exceptions import (
    InsertError,
    PoolError,
    ServerError,
    AuthenticationError,
    UserAuthError)


class Client:
    def __init__(self, db, servers, username='iris', password='siri'):
        self.db = db

        if isinstance(servers, Server):
            servers = [servers]

        self.cluster = AsyncSiriCluster(
            username=username,
            password=password,
            dbname=db.dbname,
            hostlist=[
                (server.listen_client_address, server.listen_client_port)
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

    async def insert_some_series(self, series, n=None, points=functools.partial(gen_points, n=1)):
        random.shuffle(series)

        if n is None:
            n = len(series) // 100

        data = {s.name: s.add_points(points()) for s in series[:n]}

        await self.insert(data)

        for s in series[:n]:
            s.commit_points()

