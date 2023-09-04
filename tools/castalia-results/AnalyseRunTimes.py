#!/usr/bin/env python3

import os.path
import argparse
import sys
import glob
from datetime import datetime, timedelta
import itertools
import statistics


def parse_args():
    parser = argparse.ArgumentParser(description="Analyse run times from castalia.log files")
    parser.add_argument('-f', '--folder', type=str, required=False, default='./',
                        help="Path to folder containing castalia.log files")
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()

    if not os.path.isdir(args.folder):
        sys.exit("Input folder {} does not exist".format(args.folder))

    # recursive=True means, in Python 3.5+, ** is parsed as 'any subfolder'
    results_files = glob.glob('{}**/{}'.format(args.folder, 'castalia.log'), recursive=True)

    if not results_files or len(results_files) == 0:
        sys.exit("No files named castalia.log found in {}".format(args.folder))

    log_results = []

    for results_file in results_files:

        # Get directory name holding results file so we can print message
        results_subfolder_name = results_file.split('/')[-2]
        print("Processing file from folder {}".format(results_subfolder_name))

        with open(results_file, 'r') as results_reader:

            for line in results_reader:

                # 2018-04-24 12:18:13: Run ID 452 label "CtpRicer,General,varyLocation,varyCellSize,varyCapSize,
                # varyMonth","traceFile="/vagrant/data/PANGEA/Lerwick/Lerwick-Combined-1-Minute.csv",cellSize=2,
                # capSize=40,solarStart=-2707200" rep "0" complete in 0:01:11

                if "complete in" in line:
                    # Remove quotes to make things easier:
                    line = line.replace('"', '')
                    line_tokens = line.split(' ')
                    dt = datetime.strptime(line_tokens[-1].strip(), "%H:%M:%S")
                    run_time_delta = timedelta(hours=dt.hour, minutes=dt.minute, seconds=dt.second)
                    labels = line_tokens[6].split(',')

                    # Keep only these labels
                    labels = [labels[index] for index in [0, 7, 8]]

                    log_results.append([run_time_delta, labels])

    # Sort by the labels
    sorted_log_results = sorted(log_results, key=lambda result: result[1])

    print("labels    sum    min      avg        max")

    for key, group in itertools.groupby(sorted_log_results, key=lambda x:x[1]):

        list_of_times = [grouped[0] for grouped in list(group)]
        sum_run_times = sum(list_of_times, timedelta())
        avg_run_times = sum_run_times / len(list_of_times)
        max_run_times = max(list_of_times)
        min_run_times = min(list_of_times)

        print("{} {}    {}    {}     {}".format(key, sum_run_times, min_run_times,  avg_run_times, max_run_times))
