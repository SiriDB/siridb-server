import unittest

class TestBase(unittest.TestCase):

    title = 'No title set'

    async def run():
        raise NotImplementedError()

