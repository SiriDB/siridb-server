BUILDTYPE = 'Debug'
TEST_DIR = './testdir'
SIRIDBC = '../{BUILDTYPE}/siridb-server'
MANAGE = ''  # '/usr/sbin/siridb-manage'
ADMIN = '/usr/local/bin/siridb-admin'
TOOL = 'ADMIN'
# VALGRIND = 'valgrind' + \
#   '--tool=memcheck ' + \
#   '--leak-check=full ' + \
#   '--show-leak-kinds=all ' + \
#   '--track-origins=yes -v'
VALGRIND = 'valgrind --tool=memcheck '
MAX_OPEN_FILES = 512    # Default value is 32768 but with valgrind 512 is max
