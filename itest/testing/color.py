
NORMAL = '\x1B[0m'
RED = '\x1B[31m'
GREEN = '\x1B[32m'
YELLOW = '\x1B[33m'
LYELLOW = '\x1b[93m'


class Color:

    @staticmethod
    def success(text):
        return f'{GREEN}{text}{NORMAL}'

    @staticmethod
    def warning(text):
        return f'{YELLOW}{text}{NORMAL}'

    @staticmethod
    def error(text):
        return f'{RED}{text}{NORMAL}'

    @staticmethod
    def info(text):
        return f'{LYELLOW}{text}{NORMAL}'
