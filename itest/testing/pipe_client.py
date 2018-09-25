import logging
import asyncio
from siridb.connector import SiriDBProtocol
from siridb.connector.lib.connection import SiriDBAsyncConnection


class PipeClient(SiriDBAsyncConnection):
    def __init__(self, pipe_name, loop=None):
        self._pipe_name = pipe_name
        self._protocol = None

    async def connect(self, username, password, dbname, loop=None):
        loop = loop or asyncio.get_event_loop()

        transport, self._protocol = await loop.create_unix_connection(
            path=self._pipe_name,
            protocol_factory=lambda: SiriDBProtocol(
                username, password, dbname))

        try:
            res = await self._protocol.auth_future
        except Exception as exc:
            logging.debug('Authentication failed: {}'.format(exc))
            transport.close()
            raise exc
        else:
            self._protocol.on_authenticated()
