#!/usr/bin/env python3

import sys, os
from glob import glob

def glob_current_dir():
    files = glob('*.cpp')
    files += glob('*.cc')
    files += glob('*.c')
    return files

def glob_and_print():
    files = glob_current_dir()
    if not files:
        return
    for f in files[:-1]:
        print(f)
    print(files[-1], end='')

if __name__ == '__main__':
    glob_and_print()
