#!/usr/bin/env python

import os.path
import argparse
import sys
import csv
from collections import defaultdict
from datetime import datetime
from datetime import timedelta
import plotly
import plotly.graph_objs as graph_objs
from plotly import tools
import matplotlib.pyplot as plt
from matplotlib.dates import HourLocator, DateFormatter

def parse_args():
    parser = argparse.ArgumentParser(description="Compare plots of data at different levels of downsampling"
                                                 "using data generated from DownsamplePangeaData")
    parser.add_argument('-i', '--infolder', type=str, required=False, default='./',
                        help="Path to folder containing 1 level of subdirectories, each subdir a separate station. "
                             "Defaults to current directory")
    parser.add_argument('-g', '--granularities', type=int, required=False, nargs='*', dest='granularities',
                        default=[1, 30, 60, 180, 360, 720],
                        help='Granularities to compare. Defaults to 1 30 60 180 360 720 '
                             '(1 minute, half hour, 1 hour, 3 hours, 6 hours, 12 hours)')
    parser.add_argument('-d', '--day', type=int, required=True,
                        help='Day to compare. This is a day of the year. E.g. 1st of March is day 59 (assuming not a'
                             'leap year)')
    return parser.parse_args()




if __name__ == '__main__':
    args = parse_args()

    if not os.path.isdir(args.infolder):
        print "Input folder does not exist"
        sys.exit()

    _granularity_to_label = {1: "Minutely", 10: "10 minutes", 30: "Half hour", 60: "Hourly", 180: "3 hours",
                             360: "6 hours", 720: "12 hours"}

    # Form the day of year specified, work out the starting and ending number of seconds of the year for that day
    seconds_in_day = 24*60*60  # 24 hours * 60 minutes * 60 seconds

    # If we want to just get daylight, start for example at midnight of the specified day
    # + 7 hours (25200 seconds) and end
    # 1 day later - 7 hours
    start_seconds = (args.day * seconds_in_day) + 25200
    end_seconds = ((args.day + 1) * seconds_in_day) - 25200

    # We will be converting elapsed time (seconds of the year) into date objects
    # To do this we need a base datetime starting from Jan 1st midnight. Using 2017 as an arbitrary year
    base_datetime = datetime(year=2017, month=1, day=1, minute=0, second=0)

    # Scan the infolder
    for root, sub_dirs, root_files in os.walk(args.infolder):

        # Data files should be in the following structure:
        # infolder
        #   |
        #   - Camborne
        #       |
        #       - Camborne-Combined-1-Minute.csv
        #       - Camborne-Combined-30-Minutes.csv
        #       - Camborne-Combined-60-Minutes.csv
        #       - Camborne-Combined-180-Minutes.csv
        #       - ... other files
        #   - Lerwick
        #       |
        #       - Camborne-Combined-1-Minute.csv
        #       - ... other files

        # Loop over every subdirectory found
        for sub_dir in sub_dirs:

            ##################
            # Extract the data

            sub_dir_path = os.path.join(root, sub_dir)
            subplots_out_path = os.path.join(sub_dir_path, "Granularity-Comparison.html")
            SVG_out_path = os.path.join(sub_dir_path, "Granularity-Comparison.svg")
            # Get a list of all the files in the sub_dir
            files = list(os.walk(sub_dir_path))[0][2]

            # A dictionary, each key represents a granularity
            # each value is another dictionary, containing graph data. Keys are for example 'x' and 'y'
            graph_data = defaultdict(lambda: defaultdict(list))

            # For every granularity specified
            for granularity in args.granularities:

                # find matches for the file we are looking for
                matches = [s for s in files if "Combined-{}-Minute".format(granularity) in s]

                # we should only find 1 match
                if len(matches) != 1:
                    sys.exit("Found unexpected number of files matching search: {}".format(len(matches)))

                # Get the full path of the file
                file_path = os.path.join(sub_dir_path, matches[0])  # [0] because we only expect 1 match, first in list

                # Open the file for reading
                with open(file_path, 'rb') as data_file_to_read:

                    data_csv_reader = csv.reader(data_file_to_read, delimiter=' ')

                    for row in data_csv_reader:
                        seconds = float(row[0])

                        # Grab the chunk of time we're interested in
                        if start_seconds <= seconds <= end_seconds:
                            # Create a datetime object out of the elapsed seconds data
                            date_time = base_datetime + timedelta(seconds=seconds)
                            # Add to the graph data
                            graph_data[granularity]['x'].append(date_time)
                            graph_data[granularity]['y'].append(float(row[1]))  # row[1] is the radiation data

            #######################
            # PLOT the data as HTML

            # We want to plot subplots on one row, each column is a different granularity
            fig = tools.make_subplots(rows=1, cols=len(args.granularities))
            subplot_counter = 1

            for granularity in args.granularities:
                fig.append_trace(
                    graph_objs.Scatter(
                        x=graph_data[granularity]['x'],
                        y=graph_data[granularity]['y'],
                        name=granularity),

                    row=1,  # Row
                    col=subplot_counter  # Column
                )

                subplot_counter += 1

            plotly.offline.plot(fig, filename=subplots_out_path, auto_open=False)


            ################
            # PLOT the data using Matplotlib (for publication)

            subplot_counter = 1
            axes = None

            # print(plt.style.available)
            # 'seaborn-darkgrid', u'Solarize_Light2', u'seaborn-notebook', u'classic', u'seaborn-ticks', u'grayscale',
            # u'bmh', u'seaborn-talk', u'dark_background', u'ggplot', u'fivethirtyeight', u'_classic_test',
            # u'seaborn-colorblind', u'seaborn-deep', u'seaborn-whitegrid', u'seaborn', u'seaborn-poster',
            # u'seaborn-bright', u'seaborn-muted', u'seaborn-paper', u'seaborn-white', u'fast', u'seaborn-pastel',
            # u'seaborn-dark', u'seaborn-dark-palette'
            #plt.style.use(['seaborn-darkgrid'])

            for granularity in args.granularities:

                # Matplotlib uses plt.subplot command to create subplots
                # this is (no_of_rows, no_of_columns, this_plot_number)
                # where this_plot_number begins at 1, and increments across columns of the first row,
                # then onto the second row etc.

                # To make the subplots share the same axes, we have to save the axes object
                # given by the first call to subplot
                if subplot_counter == 1:
                    axes = plt.subplot(1, len(args.granularities), subplot_counter)
                # Then set this as sharex and sharey arguments of subsequent plots
                else:
                    axes = plt.subplot(1, len(args.granularities), subplot_counter, sharex = axes, sharey=axes)
                    axes.yaxis.set_visible(False)

                # Format the date axis
                axes.xaxis.set_major_locator(HourLocator(interval=4))
                axes.xaxis.set_major_formatter(DateFormatter("%l%p"))

                plt.plot(graph_data[granularity]['x'],
                         graph_data[granularity]['y'],
                         '-')  # line type
                plt.title(_granularity_to_label[granularity])
                plt.ylabel('W/m2')

                subplot_counter += 1

            plt.show()
