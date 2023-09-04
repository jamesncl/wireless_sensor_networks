#!/usr/bin/env python3

import os
import argparse
import sys
from random import randint
import glob


def parse_args():
    parser = argparse.ArgumentParser(description="Sort given directory of files evenly into given number of subfolders. "
                                     "")
    parser.add_argument('--proportional', action='store_true', required=False, default=False,
                        help='If set nodes 13-24 WILL HAVE HALF AS MANY because assuming they are only 4 core VMs')
    parser.add_argument('--skip', type=int, required=False, nargs='*', dest='skip_these_nodes',
                        help='Skip the specified nodes (do not create dirs and do not move files to them)')
    parser.add_argument('-f', '--folder', type=str, required=True,
                        help="Path to folder containing files")
    parser.add_argument('-n', '--number', type=int, required=True,
                        help='Number of subfolders to generate')
    parser.add_argument('--whatif', action='store_true', required=False, default=False,
                        help='Do not move files or create folders, just print what would happen')
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

    # parse run ID from config_file name e.g. ./omnetpp.990.tmp
    config_files_with_runid = []
    for config_file in config_files:
        split = config_file.split('.')
        config_files_with_runid.append([config_file, int(split[-2])])

    # Sort by the run ID
    config_files_with_runid = sorted(config_files_with_runid, key=lambda x: x[1])

    # Create subfolders
    for i in range(args.number):

        folder_number = i+1  # + 1 because range() is zero based

        if args.skip_these_nodes is None or folder_number not in args.skip_these_nodes:
            folder_to_create = os.path.join(args.folder, str(folder_number))
            print("Create folder {}".format(folder_to_create))

            if not args.whatif:
                os.mkdir(folder_to_create)
        else:
            print("skipping folder {}".format(i+1))

    file_counter = 0
    flip_flop = 0

    for file_and_id_to_move in config_files_with_runid:

        folder_to_move_to = (file_counter % args.number) + 1

        # Hack to distribute only HALF the number of files into nodes 13-24
        # because these VMs only have 4 cores
        if args.proportional and folder_to_move_to == 13:
            if flip_flop == 1:
                file_counter = file_counter + 12
                folder_to_move_to = (file_counter % args.number) + 1

            flip_flop = 0 if flip_flop == 1 else 1

        # Skip any folders we were told to
        if args.skip_these_nodes is not None and folder_to_move_to in args.skip_these_nodes:
            file_counter = file_counter + 1
            folder_to_move_to = (file_counter % args.number) + 1

        print("Moving {} to {}".format(file_and_id_to_move[0], folder_to_move_to))
        file_counter = file_counter + 1

        if not args.whatif:
            os.rename(file_and_id_to_move[0], os.path.join(str(folder_to_move_to), file_and_id_to_move[0]))