import time
import random

TP_INT = 1
TP_FLOAT = 0

class Points:
    def __init__(self, tp):
        self.tp = tp




p = Points(TP_INT)


a = int(time.time())
b = int((time.time() - 3600 * 24 * 6) - random.randint(1, 3600))

print(bin(a), bin(b))
print(bin(a ^ b))

def compress(points):
    pass

'''

001101000111001
001101000011011
001101001111011

111111111011101


000000000100000

111111111011111


'''

