#!/usr/bin/env python
import struct
import argparse
import asyncio
import qpack


class TeeServerProtocol(asyncio.Protocol):
    def connection_made(self, transport):
        peername = transport.get_extra_info('peername')
        print('Connection from {}'.format(peername))
        self.transport = transport

    def data_received(self, data):
        print('Data received: {}'.format(len(data)))
        header = struct.unpack_from('<LHBB', data)
        print(header)
        # self.transport.write(b'test')


async def main(args):
    loop = asyncio.get_running_loop()

    server = await loop.create_server(
        lambda: TeeServerProtocol(),
        '0.0.0.0',
        args.port)

    async with server:
        await server.serve_forever()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=9104)
    args = parser.parse_args()

    loop = asyncio.new_event_loop()
    loop.run_until_complete(main(args))
