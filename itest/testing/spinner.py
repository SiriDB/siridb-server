SPINNER1 = \
    ('▁', '▂', '▃', '▄', '▅', '▆', '▇', '█', '▇', '▆', '▅', '▄', '▃', '▁')
SPINNER2 = \
    ('⠁', '⠂', '⠄', '⡀', '⢀', '⠠', '⠐', '⠈')
SPINNER3 = \
    ('◐', '◓', '◑', '◒')


class Spinner():

    def __init__(self, charset=SPINNER3):
        self._idx = 0
        self._charset = charset
        self._len = len(charset)

    @property
    def next(self):
        char = self._charset[self._idx]
        self._idx += 1
        self._idx %= self._len
        return char
