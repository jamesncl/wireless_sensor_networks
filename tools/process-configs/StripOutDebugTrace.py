#!/usr/bin/env python3

import os
import argparse
import sys
from random import randint
import glob


def parse_args():
    parser = argparse.ArgumentParser(description="Remove lines containing collectTraceInfo or collectPlotTraceInfo")
    parser.add_argument('-f', '--folder', type=str, required=True,
                        help="Path to folder containing files")
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

    for config_file in config_files:
        with open(config_file, "r") as config_reader:
            lines = config_reader.readlines()

        with open(config_file, "w") as config_writer:
            for line in lines:
                if 'collectTraceInfo' not in line and 'collectPlotTraceInfo' not in line:
                    config_writer.write(line)