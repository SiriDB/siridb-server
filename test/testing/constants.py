BUILDTYPE = 'Debug'
TEST_DIR = '/home/joente/workspace/testdir'
SIRIDBC = '/home/joente/workspace/siridb-server/{BUILDTYPE}/siridb-server'
MANAGE = '/home/joente/workspace/siridb-manage/main.py'
VALGRIND = 'valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes -v '
# VALGRIND = 'valgrind --tool=memcheck '
MAX_OPEN_FILES = 32768    # Default value is 32768 but with valgrind 512 is max