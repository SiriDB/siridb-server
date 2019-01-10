
import logging
import asyncio
import struct
import qpack
from siridb.connector import SiriDBProtocol
from siridb.connector.lib.connection import SiriDBAsyncConnection


class Package:

    __slots__ = ('pid', 'length', 'tipe', 'checkbit', 'data')

    struct_datapackage = struct.Struct('<IHBB')

    def __init__(self, barray):
        self.length, self.pid, self.tipe, self.checkbit = \
            self.__class__.struct_datapackage.unpack_from(barray, offset=0)
        self.length += self.__class__.struct_datapackage.size
        self.data = None

    def extract_data_from(self, barray):
        try:
            self.data = qpack.unpackb(
                barray[self.__class__.struct_datapackage.size:self.length],
                decode='utf-8')
        finally:
            del barray[:self.length]


class SiriDBServerProtocol(asyncio.Protocol):

    def __init__(self, on_package_received):
        self._buffered_data = bytearray()
        self._data_package = None
        self._on_package_received = on_package_received

    def data_received(self, data):
        '''
        override asyncio.Protocol
        '''
        self._buffered_data.extend(data)
        while self._buffered_data:
            size = len(self._buffered_data)
            if self._data_package is None:
                if size < Package.struct_datapackage.size:
                    return None
                self._data_package = Package(self._buffered_data)
            if size < self._data_package.length:
                return None
            try:
                self._data_package.extract_data_from(self._buffered_data)
            except KeyError as e:
                logging.error('Unsupported package received: {}'.format(e))
            except Exception as e:
                logging.exception(e)
                # empty the byte-array to recover from this error
                self._buffered_data.clear()
            else:
                self._on_package_received(self._data_package.data)
            self._data_package = None


class PipeServer(SiriDBAsyncConnection):
    def __init__(self, pipe_name, on_data):
        self._pipe_name = pipe_name
        self._protocol = None
        self._server = None
        self._on_data_cb = on_data

    async def create(self, loop=None):
        loop = loop or asyncio.get_event_loop()

        self._server = await loop.create_unix_server(
            path=self._pipe_name,
            protocol_factory=lambda: SiriDBServerProtocol(self._on_data))

    def _on_data(self, data):
        '''
        series names are returned as c strings (0 terminated)
        '''
        data = {k.rstrip('\x00'): v for k, v in data.items()}
        self._on_data_cb(data)
