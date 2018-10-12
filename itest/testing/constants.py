TEST_DIR = './testdir'
SIRIDBC = '../{BUILDTYPE}/siridb-server'
SERVICE = '/usr/local/bin/siridb-admin'
VALGRIND = 'valgrind' \
  ' --tool=memcheck' \
  ' --error-exitcode=1' \
  ' --leak-check=full' \
  ' --show-leak-kinds=all' \
  ' --track-origins=yes' \
  ' -v '
VALGRIND = 'valgrind' \
  ' --tool=memcheck'  \
  ' --error-exitcode=1' \
  ' --leak-check=full '

MAX_OPEN_FILES = 512    # Default value is 32768 but with valgrind 512 is max
