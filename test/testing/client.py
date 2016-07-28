import sys
import logging
from .constants import PY_SIRIDB_PATH
from .server import Server

sys.path.append(PY_SIRIDB_PATH)
from siriclient import AsyncSiriCluster
from siriclient.shared.exceptions import (
    InsertError,
    PoolError,
    AuthenticationError)


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

    async def connect(self):
        logging.info('Create client connection to database {}'.format(
            self.db.dbname))
        await self.cluster.connect()

    def close(self):
        logging.info('Closing connection to database {}'.format(
            self.db.dbname))
        self.cluster.close()