#!/usr/bin/python3.5
'''

Build key with:

    see: http://blog.jonliv.es/blog/2011/04/26/
        creating-your-own-signed-apt-repository-and-debian-packages/

    Run the following command: gpg --gen-key
    Select Option 4 to generate an RSA Key
    Select 0, key does not expire
    Choose your keysize from the options given
    (doesn’t really matter for our purposes)
    Enter your name and email address when prompted

Sign packages:
    dpkg-sig --sign builder *.deb

Add to repo:
    reprepro --ask-passphrase -Vb . includedeb vivid /path/to/package.deb

Install public key:
    wget -O - http://storage.googleapis.com/siridb_repo/
        jeroen@transceptor.technology.gpg.key | sudo apt-key add -

'''

import os
import sys
import subprocess
import shutil
import argparse
import pytz
sys.path.append('server')
sys.path.append('/home/joente/workspace/pyleri')
from siridb.version import CURRENT_VERSION  # nopep8
from siridb.constants import PYTZ_VERSION  # nopep8
from trender import TRender  # nopep8
import app.grammar  # nopep8

yestoall = False


def get_answer(s):
    global yestoall
    if yestoall:
        return True
    while True:
        answer = input('{} (y/n/a/q) '.format(s))
        if answer == 'a':
            yestoall = True
            return True
        if answer == 'y':
            return True
        if answer == 'n':
            return False
        if answer == 'q':
            sys.exit('Quit... bye!')


def clean_build_folders():
    for path in ('manage', 'server', 'prompt'):
        for remove_path in next(os.walk(os.path.join(path, 'build')))[1]:
            print('remove path: {}'.format(
                os.path.join(path, 'build', remove_path)))
            shutil.rmtree(os.path.join(path, 'build', remove_path))
    print('\nFinished cleaning build folders...\n')


def update_shared_folders():
    for path in ('siriclient', 'siriclienttwisted', 'siritwisted'):
        shared_path = os.path.join(path, 'shared')
        if os.path.exists(shared_path):
            print('remove path: {}'.format(shared_path))
            shutil.rmtree(shared_path)
        os.mkdir(shared_path)
        print('copy file: {}'.format(
            os.path.join(shared_path, 'exceptions.py')))
        shutil.copyfile(os.path.join('server', 'siridb', 'exceptions.py'),
                        os.path.join(shared_path, 'exceptions.py'))
        print('copy file: {}'.format(
            os.path.join(shared_path, 'constants.py')))
        shutil.copyfile(os.path.join('server', 'siridb', 'constants.py'),
                        os.path.join(shared_path, 'constants.py'))
        print('copy directory: {}'.format(
            os.path.join(shared_path, 'packageprotocol')))
        shutil.copytree(os.path.join('server', 'packageprotocol'),
                        os.path.join(shared_path, 'packageprotocol'))
        print('write to file: {}'.format(
            os.path.join(shared_path, '__init__.py')))
        with open(os.path.join(shared_path, '__init__.py'),
                  'w',
                  encoding='utf-8') as f:
            f.write("""'''
This modules contains files from the SiriDB project and will be replaced on
each deploy.

WARNING: DO NOT MODIFY ANY FILE IN THIS FOLDER SINCE CHANGES WILL BE
         LOST ON DEPLOY
'''
""")
    print('\nFinished updating shared folders...\n')


def compile_js_grammar():
    with open(os.path.join('server', 'static', 'js', 'grammar.js'),
              'w',
              encoding='utf-8') as f:
        f.write(app.grammar.siriGrammar.export_js())
    print('\nFinished creating a new grammar.js file...\n')


def compile_c_grammar():
    c_file, h_file = app.grammar.siriGrammar.export_c(target='siri/grammar/grammar')
    with open(os.path.join('workfiles', 'c', 'grammar.c'),
              'w',
              encoding='utf-8') as f:
        f.write(c_file)
    with open(os.path.join('workfiles', 'c', 'grammar.h'),
              'w',
              encoding='utf-8') as f:
        f.write(h_file)

    print('\nFinished creating new c- grammar files...\n')


def update_timezones():
    template_path = os.path.join('server', 'help', 'en')
    template = TRender('help_timezones.template', path=template_path)
    with open(os.path.join(template_path, 'help_timezones.md'),
              'w',
              encoding='utf-8') as f:
        f.write(template.render({'timezones': [
            tz.replace('_', '\_') for tz in pytz.common_timezones]}))
    print('\nFinished creating time-zones help file\n')


def compile_ciri_module():
    subprocess.call(['python3.5', 'setup.py', 'build_ext', '--inplace'],
                    cwd=os.path.join('server', 'ciri'))  # '--force'


def compile_ccsv_module():
    subprocess.call(['python3.5', 'setup.py', 'build_ext', '--inplace'],
                    cwd=os.path.join('server', 'ccsv'))  # '--force'


def minify_css_and_js():
    from csscompressor import compress
    from slimit import minify
    #################################################
    # Minify app js
    #################################################

    content = []
    with open(os.path.join('server', 'static', 'js', 'grammar.js'),
              'r') as f:
        content.append(f.read())

    with open(os.path.join('server', 'static', 'js', 'app.js'),
              'r') as f:
        content.append(f.read())

    with open(os.path.join('server', 'static', 'js', 'router.js'),
              'r') as f:
        content.append(f.read())

    with open(os.path.join('server', 'static', 'js', 'app.min.js'),
              'w') as f:
        f.write(minify(''.join(content), mangle=True, mangle_toplevel=True))

    with open(os.path.join('server', 'static', 'js', 'lib', 'oz.js'),
              'r') as f:
        content = f.read()

    with open(os.path.join('server', 'static', 'js', 'lib', 'oz.min.js'),
              'w') as f:
        f.write(minify(content, mangle=True, mangle_toplevel=True))

    #################################################
    # Minify css
    #################################################

    with open(os.path.join('server', 'static', 'css', 'style.css'),
              'r') as f:
        content = f.read()

    with open(os.path.join('server', 'static', 'css', 'style.min.css'),
              'w') as f:
        f.write(compress(content))

    print('\nFinished minifying css and js files\n')


def compile_siridb_manage(auto_build=False):
    subprocess.call(['python3', 'setup.py', 'build'], cwd='manage/')

    if auto_build or get_answer('build siridb-manage deb package?'):
        subprocess.call(['python3', 'build_deb.py', 'siridb-manage'])
    print('\nFinished creating SiriDB Manage {}\n'.format(CURRENT_VERSION))


def compile_siridb_server(auto_build=False):
    subprocess.call(['python3', 'setup.py', 'build'], cwd='server/')

    if auto_build or get_answer('build siridb-server deb package?'):
        subprocess.call(['python3', 'build_deb.py', 'siridb-server'])
    print('\nFinished creating SiriDB Server {}\n'.format(CURRENT_VERSION))


def compile_siridb_prompt(auto_build=False):
    subprocess.call(['python3', 'setup.py', 'build'], cwd='prompt/')

    if auto_build or get_answer('build siridb-prompt deb package?'):
        subprocess.call(['python3', 'build_deb.py', 'siridb-prompt'])
    print('\nFinished creating SiriDB Prompt {}\n'.format(CURRENT_VERSION))


def everything():
    if get_answer('Do you want to clean the build folders first?'):
        clean_build_folders()

    if get_answer('Do you want to update the shared folders?'):
        update_shared_folders()

    if get_answer('Do you want to generate a new javascript grammar.js file?'):
        compile_js_grammar()

    if get_answer('Do you want to generate new c grammar files?'):
        compile_c_grammar()

    if get_answer('Do you want to generate a time zones help file?'):
        update_timezones()

    if get_answer('Do you want to compile a new ciri module?'):
        compile_ciri_module()

    if get_answer('Do you want to compile a new ccsv module?'):
        compile_ccsv_module()

    if get_answer('Do you want to minify css and js files?'):
        minify_css_and_js()

    if get_answer('compile siridb-manage? (update the change-log first!!)'):
        compile_siridb_manage()

    if get_answer('compile siridb-server?'):
        compile_siridb_server()


if __name__ == '__main__':
    # check for correct pytz module since its important that we know all
    # expecting time zones.
    if PYTZ_VERSION != pytz.__version__:
        raise ImportError('pytz module is version {} but we expect version {}'
                          .format(pytz.__version__, PYTZ_VERSION))

    parser = argparse.ArgumentParser()
    parser.add_argument('--clean-build-folder',
                        action='store_true',
                        help='Cleanup build folders')
    parser.add_argument('--update-shared-folders',
                        action='store_true',
                        help='Update shared folders')
    parser.add_argument('--timezones',
                        action='store_true',
                        help='Update time-zones help file')
    parser.add_argument('--js-grammar',
                        action='store_true',
                        help='Compile new javascript grammar.js file')
    parser.add_argument('--c-grammar',
                        action='store_true',
                        help='Compile new c grammar files')
    parser.add_argument('--ciri',
                        action='store_true',
                        help='Compile new ciri module')
    parser.add_argument('--ccsv',
                        action='store_true',
                        help='Compile new ccsv module')
    parser.add_argument('--minify',
                        action='store_true',
                        help='Minify css and js files')
    parser.add_argument('--manage',
                        action='store_true',
                        help='Compile SiriDB Manage package')
    parser.add_argument('--server',
                        action='store_true',
                        help='Compile SiriDB Server package')
    parser.add_argument('--prompt',
                        action='store_true',
                        help='Compile SiriDB Prompt package')

    args = parser.parse_args()

    if args.clean_build_folder:
        clean_build_folders()

    if args.update_shared_folders:
        update_shared_folders()

    if args.timezones:
        update_timezones()

    if args.js_grammar:
        compile_js_grammar()

    if args.c_grammar:
        compile_c_grammar()

    if args.ciri:
        compile_ciri_module()

    if args.ccsv:
        compile_ccsv_module()

    if args.minify:
        minify_css_and_js()

    if args.manage:
        compile_siridb_manage(auto_build=True)

    if args.server:
        compile_siridb_server(auto_build=True)

    if args.prompt:
        compile_siridb_prompt(auto_build=True)

    if not any(vars(args).values()):
        everything()
