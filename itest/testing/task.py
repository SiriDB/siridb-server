import sys
import time
import asyncio
from .spinner import Spinner
from .color import Color


class Task:
    def __init__(self, title):
        self.running = True
        self.task = asyncio.ensure_future(self.process())
        self.success = False
        self.title = title
        self.start = time.time()

    def stop(self, success):
        self.running = False
        self.success = success
        self.duration = time.time() - self.start

    async def process(self):
        spinner = Spinner()
        while self.running:
            sys.stdout.write(f'{self.title:.<76}{spinner.next}\r')
            sys.stdout.flush()
            await asyncio.sleep(0.2)

        if self.success:
            print(
                f'{self.title:.<76}'
                f'{Color.success("OK")} ({self.duration:.2f} seconds)')
        else:
            print(
                f'{self.title:.<76}'
                f'{Color.error("FAILED")} ({self.duration:.2f} seconds)')
