#!/usr/bin/python3

import sys
import os
import datetime
import platform
import subprocess
import shutil
import stat
import re
import argparse

VERSION_FILE = 'include/siri/version.h'
CHANGELOG_FILE = 'ChangeLog'

def _version_levels():
    for n in ('MAJOR', 'MINOR', 'PATCH'):
        yield re.compile('^#define SIRIDB_VERSION_{} ([0-9]+)$'.format(n))

def _get_version():
    version_gen = _version_levels()
    version_m = next(version_gen)
    version = []
    with open(VERSION_FILE, 'r') as f:
        content = f.readlines()

    for line in content:
        m = version_m.match(line)
        if m:
            version.append(m.group(1))
            try:
                version_m = next(version_gen)
            except StopIteration:
                return '.'.join(version)

    raise ValueError('Cannot find version in {}'.format(VERSION_FILE))

def _get_changelog(version):
    with open('ChangeLog-{}'.format(version), 'r') as f:
        content = f.read()
    if not content:
        raise ValueError('Changelog required!')
    return content

def _get_distribution():
    '''Returns distribution code name. (Ubuntu)'''
    proc = subprocess.Popen(['lsb_release', '-c'], stdout=subprocess.PIPE)
    for line in proc.stdout:
        if line:
            return line.decode().split('\t')[1].strip()


if __name__ == '__main__':
    # Read the current version
    version = _get_version()
    if version is None:
        exit('Cannot find version in file: {}'.format(VERSION_FILE))

    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-r',
        '--rev',
        type=int,
        default=0,
        help='Debian Revision number.')

    parser.add_argument(
        '-f',
        '--force',
        action='store_true',
        help='Overwrite existing build.')

    args = parser.parse_args()

    if args.rev:
        version += '-{}'.format(args.rev)

    # Explain architecture= amd64
    # The architecture is AMD64-compatible and Debian AMD64 will run on AMD and
    # Intel processors with 64-bit support.
    # Because of the technology paternity, Debian uses the name "AMD64".
    config = dict(
        package='siridb-server',
        version=version,
        name='Jeroen van der Heijden',
        email='jeroen@transceptor.technology',
        company='Transceptor Technology',
        company_email='info@transceptor.technology',
        datetime=datetime.datetime.utcnow().strftime(
                '%a, %d %b %Y %H:%M:%S') + ' +0000',
        architecture={
            '32bit': 'i386',
            '64bit': 'amd64'}[platform.architecture()[0]],
        archother={
            '32bit': 'i386',
            '64bit': 'x86_64'}[platform.architecture()[0]],
        homepage='http://siridb.net',
        distribution=_get_distribution(),
        curdate=datetime.datetime.utcnow().strftime('%d %b %Y'),
        year=datetime.datetime.utcnow().year,
        description='SiriDB time series database server',
        long_description='''
 SiriDB is a fast and scalable time series database.
        '''.rstrip(),
        explain='start the SiriDB time series database server',
        depends='${shlibs:Depends}, '
                '${misc:Depends}, '
                'libuv1 (>= 1.8.0)'
    )

    with open(CHANGELOG_FILE, 'r') as f:
        current_changelog = f.read()

    if '{package} ({version})'.format(
            **config) in current_changelog:
        if not args.force:
            raise ValueError(
                'Version {} already build. Use -r <revision> to create a new '
                'revision number or use -f to overwrite the existing pacakge'
                .format(version))
        changelog = None
    else:
        changelog = _get_changelog(version)
        config.update(dict(
            changelog=changelog.strip()
        ))

    POSTINST = open(
        'deb/POSTINST', 'r').read().strip().format(**config)
    SYSTEMD = open(
        'deb/SYSTEMD', 'r').read().strip().format(**config)
    PRERM = open(
        'deb/PRERM', 'r').read().strip().format(**config)
    OVERRIDES = open(
        'deb/OVERRIDES', 'r').read().strip().format(**config)
    if changelog:
        CHANGELOG = open(
            'deb/CHANGELOG', 'r').read().strip().format(**config)
    CONTROL = open(
        'deb/CONTROL', 'r').read().strip().format(**config)
    MANPAGE = open(
        'deb/MANPAGE', 'r').read().strip().format(**config)
    COPYRIGHT = open(
        'deb/COPYRIGHT', 'r').read().strip().format(**config)
    RULES = open(
        'deb/RULES', 'r').read().strip()

    temp_path = os.path.join('build', 'temp')
    if os.path.isdir(temp_path):
        shutil.rmtree(temp_path)

    source_path = os.path.join('Release', 'siridb-server')
    if not os.path.isfile(source_path):
        sys.exit('ERROR: Cannot find path: {}'.format(source_path))

    subprocess.call(['strip', '--strip-unneeded', source_path])

    deb_file = \
        '{package}_{version}_{architecture}.deb'.format(**config)
    source_deb = os.path.join(temp_path, deb_file)
    dest_deb = os.path.join('build', deb_file)

    if os.path.exists(dest_deb):
        os.unlink(dest_deb)

    pkg_path = os.path.join(
        temp_path,
        '{package}_{version}'.format(**config))

    debian_path = os.path.join(pkg_path, 'debian')

    pkg_src_path = os.path.join(pkg_path, 'src')

    debian_source_path = os.path.join(debian_path, 'source')

    target_path = os.path.join(pkg_src_path, 'usr', 'lib', 'siridb', 'server')

    os.makedirs(target_path)
    os.makedirs(debian_source_path)

    shutil.copy2(source_path, os.path.join(target_path, config['package']))
    shutil.copytree('help', os.path.join(target_path, 'help'))

    db_path = os.path.join(pkg_src_path, 'var', 'lib', 'siridb')
    os.makedirs(db_path)

    cfg_path = os.path.join(pkg_src_path, 'etc', 'siridb')
    os.makedirs(cfg_path)
    shutil.copy('siridb.conf', cfg_path)

    systemd_path = os.path.join(target_path, 'systemd')
    os.makedirs(systemd_path)
    with open(os.path.join(
            systemd_path, '{package}.service'.format(**config)), 'w') as f:
        f.write(SYSTEMD)

    with open(os.path.join(debian_path, 'postinst'), 'w') as f:
        f.write(POSTINST)

    with open(os.path.join(debian_path, 'prerm'), 'w') as f:
        f.write(PRERM)

    with open(os.path.join(debian_path, 'source', 'format'), 'w') as f:
        f.write('3.0 (quilt)')

    with open(os.path.join(debian_path, 'compat'), 'w') as f:
        f.write('9')

    if changelog:
        changelog = CHANGELOG + '\n\n' + current_changelog
        with open(CHANGELOG_FILE, 'w') as f:
            f.write(changelog)

    shutil.copy(CHANGELOG_FILE, os.path.join(debian_path, 'changelog'))

    with open(os.path.join(debian_path, 'control'), 'w') as f:
        f.write(CONTROL)

    with open(os.path.join(debian_path, 'copyright'), 'w') as f:
        f.write(COPYRIGHT)

    rules_file = os.path.join(debian_path, 'rules')
    with open(rules_file, 'w') as f:
        f.write(RULES)

    os.chmod(rules_file, os.stat(rules_file).st_mode | stat.S_IEXEC)

    with open(os.path.join(debian_path, 'links'), 'w') as f:
        f.write('/usr/lib/siridb/server/{package} /usr/sbin/{package}\n'.format(
            **config))

    with open(os.path.join(debian_path, 'install'), 'w') as f:
        f.write('''src/usr /
src/etc /
src/var /''')

    with open(os.path.join(debian_path, '{}.1'.format(
            config['package'])), 'w') as f:
        f.write(MANPAGE)

    with open(os.path.join(debian_path, '{}.manpages'.format(
            config['package'])), 'w') as f:
        f.write('debian/{}.1'.format(config['package']))

    with open(os.path.join(debian_path, '{}.lintian-overrides'.format(
            config['package'])), 'w') as f:
        f.write(OVERRIDES)

    subprocess.call(['debuild', '-us', '-uc', '-b'], cwd=pkg_path)

    if os.path.exists(source_deb):
        shutil.move(source_deb, dest_deb)
        shutil.rmtree(temp_path)
        sys.exit('Successful created package: {}'.format(dest_deb))
    else:
        sys.exit('ERROR: {} not created'.format(source_deb))
