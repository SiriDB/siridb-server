BUILDTYPE = 'Debug'
TEST_DIR = './testdir'
SIRIDBC = '../{BUILDTYPE}/siridb-server'
MANAGE = 'siridb-manage'
# VALGRIND = 'valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes -v '
VALGRIND = 'valgrind --tool=memcheck '
MAX_OPEN_FILES = 512    # Default value is 32768 but with valgrind 512 is max
