#!/usr/bin/env python3

import os
import argparse
import sys
from random import randint
import glob


def parse_args():
    parser = argparse.ArgumentParser(description="Sort given directory of files evenly into given number of subfolders")
    parser.add_argument('-f', '--folder', type=str, required=True,
                        help="Path to folder containing files")
    parser.add_argument('-n', '--number', type=int, required=True,
                        help='Number of subfolders to generate')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()

    if not os.path.isdir(args.folder):
        sys.exit("Input folder {} does not exist".format(args.folder))

    # Check no subfolders exist
    for subdir, dirs, files in os.walk(args.folder):
        if len(dirs) > 0:
            sys.exit("Input folder contains subfolders. Exiting.")

    # recursive=True means, in Python 3.5+, ** is parsed as 'any subfolder'
    config_files = glob.glob('{}/*'.format(args.folder), recursive=True)

    if not config_files or len(config_files) == 0:
        sys.exit("No files found in {}".format(args.folder))

    # Create subfolders
    for i in range(args.number):
        os.mkdir(os.path.join(args.folder, str(i+1)))

    for file_to_move in config_files:
        random_folder = randint(1, args.number)
        os.rename(os.path.join(subdir, file_to_move), os.path.join(subdir, str(random_folder), file_to_move))