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

    def datagram_received(self, data):
        print('Data received: {}'.format(len(data)))
        header = struct.unpack_from('<LHBB', data)
        print(header)
        # self.transport.write(b'test')


async def main(args):
    loop = asyncio.get_running_loop()

    server = await loop.create_datagram_endpoint(
        lambda: TeeServerProtocol(),
        ('0.0.0.0', args.port))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=9104)
    args = parser.parse_args()

    loop = asyncio.get_event_loop()
    loop.run_until_complete(main(args))
    loop.run_forever()
