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
    parser = argparse.ArgumentParser(description="Compare minutely plots of data at different locations "
                                                 "using data generated from PreprocessPangeaData")
    parser.add_argument('-i', '--infolder', type=str, required=False, default='./',
                        help="Path to folder containing 1 level of subdirectories, each subdir a separate station. "
                             "Defaults to current directory")
    parser.add_argument('-l', '--locations', type=str, required=False, nargs='*', dest='locations',
                        default=['Lerwick', 'Tateno', 'Tamanrasset'],
                        help='Locations to compare. Defaults to Lerwick, Tateno, Tamanrasset')
    parser.add_argument('-d', '--day', type=int, required=True,
                        help='Day to compare. This is a day of the year. E.g. 1st of March is day 59 (assuming not a'
                             'leap year)')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()

    if not os.path.isdir(args.infolder):
        print "Input folder does not exist"
        sys.exit()

    # Form the day of year specified, work out the starting and ending number of seconds of the year for that day
    seconds_in_day = 24 * 60 * 60  # 24 hours * 60 minutes * 60 seconds

    # If we want to just get daylight, start for example at midnight of the specified day
    # + 7 hours (25200 seconds) and end
    # 1 day later - 7 hours
    start_seconds = (args.day * seconds_in_day) + 25200
    end_seconds = ((args.day + 1) * seconds_in_day) - 25200

    # We will be converting elapsed time (seconds of the year) into date objects
    # To do this we need a base datetime starting from Jan 1st midnight. Using 2017 as an arbitrary year
    base_datetime = datetime(year=2017, month=1, day=1, minute=0, second=0)

    # Scan the infolder
    sub_dirs = list(os.walk(args.infolder))[0][1]

    # Data files should be in the following structure:
    # infolder
    #   |
    #   - Camborne
    #       |
    #       - Camborne-Combined-1-Minute.csv
    #       - ... other files
    #   - Lerwick
    #       |
    #       - Lerwick-Combined-1-Minute.csv
    #       - ... other files

    subplots_out_path = os.path.join(args.infolder, "Location-Comparison.html")

    # A dictionary, each key represents a location
    # each value is another dictionary, containing graph data. Keys are for example 'x' and 'y'
    graph_data = defaultdict(lambda: defaultdict(list))

    # Loop over every subdirectory found
    for sub_dir in sub_dirs:

        # the sub_dir name is the location
        location = sub_dir
        print "Scanning {}".format(location)

        ##################
        # Extract the data

        sub_dir_path = os.path.join(args.infolder, sub_dir)

        # Get a list of all the files in the sub_dir
        files = list(os.walk(sub_dir_path))[0][2]

        # find matches for the file we are looking for (the minutely granularity file)
        matches = [s for s in files if "Combined-1-Minute" in s]

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

                # Grab the data we're interested in
                if start_seconds <= seconds <= end_seconds:
                    # Create a datetime object out of the elapsed seconds data
                    date_time = base_datetime + timedelta(seconds=seconds)
                    # Add to the graph data
                    graph_data[location]['x'].append(date_time)
                    graph_data[location]['y'].append(float(row[1]))  # row[1] is the radiation data

    #######################
    # PLOT the data as HTML

    # We want to plot subplots on one row, each column is a different granularity
    fig = tools.make_subplots(rows=1, cols=len(args.locations), shared_yaxes=True)
    subplot_counter = 1

    for location in args.locations:
        fig.append_trace(
            graph_objs.Scatter(
                x=graph_data[location]['x'],
                y=graph_data[location]['y'],
                name=location),

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
    # plt.style.use(['seaborn-darkgrid'])

    for location in args.locations:

        # Matplotlib uses plt.subplot command to create subplots
        # this is (no_of_rows, no_of_columns, this_plot_number)
        # where this_plot_number begins at 1, and increments across columns of the first row,
        # then onto the second row etc.

        # To make the subplots share the same axes, we have to save the axes object
        # given by the first call to subplot
        if subplot_counter == 1:
            axes = plt.subplot(1, len(args.locations), subplot_counter)
        # Then set this as sharex and sharey arguments of subsequent plots
        else:
            axes = plt.subplot(1, len(args.locations), subplot_counter, sharex=axes, sharey=axes)
            axes.yaxis.set_visible(False)

        # Format the date axis
        axes.xaxis.set_major_locator(HourLocator(interval=4))
        axes.xaxis.set_major_formatter(DateFormatter("%l%p"))

        plt.plot(graph_data[location]['x'],
                 graph_data[location]['y'],
                 '-')  # line type
        plt.title(location)
        plt.ylabel('W/m2')

        subplot_counter += 1

    plt.show()
