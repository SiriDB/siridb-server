import os
import subprocess
import argparse
from .server import Server
from .color import Color
from .constants import SIRIDBC


def is_valgrind_installed():
    with open(os.devnull, 'w') as fnull:
        try:
            subprocess.call(['valgrind'], stdout=fnull, stderr=fnull)
        except OSError as e:
            if e.errno == os.errno.ENOENT:
                return False
    return True


def print_siridb_version(args):
    fn = SIRIDBC.format(BUILDTYPE=args.build)
    try:
        p = subprocess.Popen(
            [fn, '--version'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
    except FileNotFoundError:
        print(Color.error(f'Cannot find: {fn}'))
        exit(1)

    output, err = p.communicate()
    rc = p.returncode
    if rc or err:
        print(Color.error(f'Cannot use: {fn}'))
        exit(1)

    print(f'Test with: {Color.info(output.decode().splitlines()[0])}')


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-t', '--terminal',
        choices=['xterm', 'xfce4-terminal'],
        default=None,
        help='Start SiriDB servers in a terminal. If no terminal is given '
        'process put their output in log files.')

    parser.add_argument(
        '-m', '--mem-check',
        action='store_true',
        help='Use `valgrind` for memory errors and leaks.')

    parser.add_argument(
        '-k', '--keep',
        action='store_true',
        help='Only valid when a terminal is used. This will keep the terminal '
        'open.')

    parser.add_argument(
        '-b', '--build',
        choices=['Release', 'Debug'],
        default='Release',
        help='Choose either the Release or Debug build.')

    parser.add_argument(
        '-l', '--log-level',
        default='critical',
        help='set the log level',
        choices=['debug', 'info', 'warning', 'error', 'critical'])

    args = parser.parse_args()

    has_valgrind = is_valgrind_installed()

    print_siridb_version(args)
    print("Test using valgrind for memory errors and leaks: ", end='')
    if args.mem_check and not has_valgrind:
        print(Color.warning('disabled (!! valgrind not found !!)'))
    elif not args.mem_check:
        print(Color.warning('disabled'))
    else:
        print(Color.success('enabled'))

    Server.MEM_CHECK = args.mem_check and has_valgrind
    Server.HOLD_TERM = args.keep
    Server.TERMINAL = args.terminal
    Server.BUILDTYPE = args.build
    Server.LOG_LEVEL = args.log_level.upper()
