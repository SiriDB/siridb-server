#!/usr/bin/python3
'''Export python grammar to C grammar files

Author: Jeroen van der Heijden (Transceptor Technology)
Date: 2016-10-10
'''
# import sys
# sys.path.insert(0, '../../pyleri/')
import os
from grammar import siri_grammar
from pyleri import Grammar


if __name__ == '__main__':
    c_file, h_file = siri_grammar.export_c(target='siri/grammar/grammar')

    EXPORT_PATH = 'cgrammar'

    try:
        os.makedirs(EXPORT_PATH)
    except FileExistsError:
        pass

    with open(os.path.join(EXPORT_PATH, 'grammar.c'),
              'w',
              encoding='utf-8') as f:
        f.write(c_file)
    with open(os.path.join(EXPORT_PATH, 'grammar.h'),
              'w',
              encoding='utf-8') as f:
        f.write(h_file)

    print('\nFinished creating new c-grammar files...\n')

    EXPORT_PATH = 'jsgrammar'

    try:
        os.makedirs(EXPORT_PATH)
    except FileExistsError:
        pass

    js_file = siri_grammar.export_js(js_template=Grammar.JS_WINDOW_TEMPLATE)

    with open(os.path.join(EXPORT_PATH, 'grammar.js'),
              'w',
              encoding='utf-8') as f:
        f.write(js_file)

    js_es6_file = siri_grammar.export_js()

    with open(os.path.join(EXPORT_PATH, 'SiriGrammar.js'),
              'w',
              encoding='utf-8') as f:
        f.write(js_es6_file)

    print('\nFinished creating new js-grammar files...\n')

    py_file = siri_grammar.export_py()

    EXPORT_PATH = 'pygrammar'

    try:
        os.makedirs(EXPORT_PATH)
    except FileExistsError:
        pass

    with open(os.path.join(EXPORT_PATH, 'grammar.py'),
              'w',
              encoding='utf-8') as f:
        f.write(py_file)

    print('\nFinished creating new py-grammar file...\n')

    go_file = siri_grammar.export_go()

    EXPORT_PATH = 'gogrammar'

    try:
        os.makedirs(EXPORT_PATH)
    except FileExistsError:
        pass

    with open(os.path.join(EXPORT_PATH, 'grammar.go'),
              'w',
              encoding='utf-8') as f:
        f.write(go_file)

    print('\nFinished creating new go-grammar file...\n')

    java_file = siri_grammar.export_java()

    EXPORT_PATH = 'javagrammar'

    try:
        os.makedirs(EXPORT_PATH)
    except FileExistsError:
        pass

    with open(os.path.join(EXPORT_PATH, 'SiriGrammar.java'),
              'w',
              encoding='utf-8') as f:
        f.write(java_file)

    print('\nFinished creating new java-grammar file...\n')
