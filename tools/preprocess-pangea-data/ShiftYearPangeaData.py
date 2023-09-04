#!/usr/bin/env python

import csv
import os.path
import argparse
import sys
from datetime import datetime
from datetime import timedelta
from collections import defaultdict
from PlotRadiationGraphs import plot_monthly


def parse_args():
    parser = argparse.ArgumentParser(description="Read raw solar data files downloaded from PANGEA (1 for each month), "
                                                 "shift the year by replacing e.g. '2007-' with '2008-'. This is for "
                                                 "when we need to substitute a months data which has errors with the "
                                                 "same month from a different year.")
    parser.add_argument('-i', '--infolder', type=str, required=False, default='./',
                        help="Path to folder containing 1 level of subdirectories, each subdir a separate station. "
                             "Defaults to current directory")
    parser.add_argument('--fromyear', type=str, required=True, help="From year")
    parser.add_argument('--toyear', type=str, required=True, help="To year")
    return parser.parse_args()


if __name__ == '__main__':

    args = parse_args()

    if not os.path.isdir(args.infolder):
        print "Input folder does not exist"
        sys.exit()

    # Scan the infolder
    for root, sub_dirs, root_files in os.walk(args.infolder):

        # Data files should be in the following structure:
        # infolder
        #   |
        #   - Camborne
        #       |
        #       - CAM_radiation_2007-01.tab
        #       - CAM_radiation_2007-02.tab
        #       - etc.
        #   - Lerwick
        #       |
        #       - LER_radiation_2007-01.tab
        #       - etc.

        # Loop over every subdirectory found
        for sub_dir in sub_dirs:

            #sub_dir_path = os.path.join(root, sub_dir)

            # Loop over all files in the subdirectory
            # Note that os.walk returns files in an arbitrary order (not sorted), so we must use
            # sorted() to guarantee alphabetical sorting (which will start at earliest date)
            for original_file in sorted(os.walk(sub_dir).next()[2]):  # [2] refers to just the list of files found

                # Find the year in the filename e.g. CLH_radiation_2008-01.tab
                if args.fromyear in original_file:
                    to_year_filename = original_file.replace(args.fromyear, args.toyear)

                    # Open a new CSV file to write the year-shifted data into
                    shifted_csv_file_path = os.path.join(sub_dir, to_year_filename)
                    print "Shifting file {} to {}".format(original_file, to_year_filename)

                    with open(shifted_csv_file_path, 'w') as shifted_file_writer:

                        with open(os.path.join(sub_dir, original_file), 'r') as original_file_reader:
                            for line in original_file_reader:

                                line = line.replace("{}-".format(args.fromyear), "{}-".format(args.toyear))
                                shifted_file_writer.write(line)

                else:
                    print "Ignoring file {} - does not contain {} in name".format(original_file, args.fromyear)

