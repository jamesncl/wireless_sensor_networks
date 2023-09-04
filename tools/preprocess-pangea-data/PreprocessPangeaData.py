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
                                                 "combine, strip out all columns except Short-wave downward / GLOBAL "
                                                 "radiation (SWD) and output multiple levels of time granularity")
    parser.add_argument('-i', '--infolder', type=str, required=False, default='./',
                        help="Path to folder containing 1 level of subdirectories, each subdir a separate station. "
                             "Defaults to current directory")
    parser.add_argument('-t', '--timezones', type=str, required=True,
                        help="Path to file containing station timezone info, including UTC offset")
    return parser.parse_args()


if __name__ == '__main__':
    """
        Typical usage:
        python PreprocessPangeaData.py -i "/home/james/Dropbox/PhD data archive/Solar Radiation/PANGAEA/2007 
        (after QA corrections)" -t ./Station-location-timezone.txt
    """
    args = parse_args()

    if not os.path.isdir(args.infolder):
        print "Input folder does not exist"
        sys.exit()

    if not os.path.isfile(args.timezones):
        print "Timezones file does not exist"
        sys.exit()

    # We need to adjust the timestamp in some data sets. E.g. some are in UTC, some are just off some unknown reason.
    # Using sunrise/sunset information from internet to check this
    # This is so we always have peak solar radiation around noon, and not something strange like 4am
    # Read this file and store offsets:
    station_timezone_dict = {}

    with open(args.timezones, 'rb') as station_timezones_file:
        station_timezones_reader = csv.reader(station_timezones_file, delimiter=',')

        # skip the header row
        next(station_timezones_reader, None)

        for row in station_timezones_reader:
            # File columns are:
            # Station, Country, Region, Latitude, Longitude, TimeShiftHours

            timezone_station = row[0].strip()
            timezone_offset = int(row[5].strip())
            print "Offsetting station {} by {} hours".format(timezone_station, timezone_offset)
            station_timezone_dict[timezone_station] = timezone_offset

    # A dictionary to store a summary of data quality issues to print at the end of execution
    # Each key represents an issue (e.g. garbage row). The value is another dictionary of counts -
    # the key being the file the problem was encountered, the value is the count
    data_quality_summary = defaultdict(lambda: defaultdict(int))

    # Also open a file to write garbage rows to, for quality control analysis later
    with open(os.path.join(args.infolder, "Data-Quality-Log.txt"), 'wb') as data_quality_log_file:

        data_quality_log_writer = csv.writer(data_quality_log_file, delimiter=',')

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

                sub_dir_path = os.path.join(root, sub_dir)

                # In the subdirectory, open a new CSV file to combine all files into
                combined_csv_file_path = os.path.join(sub_dir_path, "{}-Combined-1-Minute.csv".format(sub_dir))
                print "Creating file {}".format(combined_csv_file_path)

                with open(combined_csv_file_path, 'wb') as combined_csv_file:
                    combined_writer = csv.writer(combined_csv_file, delimiter=' ')

                    # We need to capture the *first* date time value in the first data file processed in this directory
                    # This is used to convert the date/time values into elapsed minutes, starting at zero
                    first_date_time_in_directory = None

                    # Plot some graphs of the data while we're at it
                    # Use these to store x/y points to plot later
                    # Use a dictionary, each entry in the dictionary represents a year/month, the value is
                    # another dictionary containing graph data lists (e.g. a list for x points, a list for y points etc)
                    # For example:
                    # {
                    #    '2007-01': {
                    #        'x': [datetime(2007,1,1,0,0,0), datetime(2007,1,1,0,1,0), ...]
                    #        'y': [0, 0, 0, 0, 0, 10, 34, 123...]
                    #    }
                    #    '2007-02:   ...
                    #        ...
                    # }
                    graph_data = defaultdict(lambda: defaultdict(list))

                    # Loop over all files in the subdirectory
                    # Note that os.walk returns files in an arbitrary order (not sorted), so we must use
                    # sorted() to guarantee alphabetical sorting (which will start at earliest date)
                    for file in sorted(os.walk(sub_dir_path).next()[2]):  # [2] refers to just the list of files found

                        # Only include .tab files, and exclude the Combined file we just created!
                        if ".tab" in file and not "Combined" in file:
                            # Open the data file to read from
                            with open(os.path.join(sub_dir_path, file), 'rb') as raw_data_csv_file:
                                raw_data_reader = csv.reader(raw_data_csv_file, delimiter='\t')

                                # There are a variable number of garbage rows at the top. We need to ignore these rows.
                                # Scan rows until we find a row with the first column containing 'Date/Time'.
                                # This is the header row, so data will follow after this row
                                # Also need to identify which column contains global short wave downward radiation (SWD)
                                found_header_row = False
                                swd_column_index = None

                                # Get the year/month from the filename, we can use this when recording graph data
                                # (so we can get a separate sub plot for each month)
                                # Filename is for example LER_radiation_2007-01.tab
                                month_year_string = file[-11:-4]

                                # For every row
                                for row in raw_data_reader:
                                    # If we haven't found the header row yet
                                    if not found_header_row:

                                        # Look for it
                                        if row[0] == 'Date/Time':
                                            found_header_row = True

                                            # Identify which column contains the solar data we need
                                            # Short-wave downward / GLOBAL radiation is column 'SWD [W/m**2]'
                                            swd_column_index = row.index('SWD [W/m**2]') # ValueError if not found

                                    # If we have found the header row, write the data row to the combined file
                                    else:

                                        data_timestamp_string = row[0]

                                        # Convert all date strings (in format 2007-03-01T00:00) to number of minutes,
                                        # starting at 0

                                        # First parse the date time value. Some garbage lines exist which throw a
                                        # ValueError, catch these and display warning and continue to next line
                                        try:
                                            parsed_datetime = datetime.strptime(data_timestamp_string, '%Y-%m-%dT%H:%M')
                                        except ValueError:
                                            # print "Warning - found garbage row ({}) in file {}".format(row, file)
                                            data_quality_summary['Un-parseable date'][file] += 1
                                            data_quality_log_writer.writerow(['Un-parseable date', file] + [row])
                                            continue

                                        # Apply the UTC offset as appropriate, according to the station timezone
                                        # sub_dir is the station name, so we use this to look up the station in the dict
                                        # Some stations do not use UTC, so are not in the dict
                                        if sub_dir in station_timezone_dict:
                                            parsed_datetime += timedelta(hours=station_timezone_dict[sub_dir])
                                        else:
                                            sys.exit("Location {} not found in timezones file".format(sub_dir))

                                        # Then check if this is the first datetime value we have found in this directory
                                        # If it is, store it so we can subtract this date when converting timestamps
                                        # to start at zero
                                        if first_date_time_in_directory is None:
                                            # If we have shifted the timestamps due to UTC offset, the data probably
                                            # isn't starting at midnight like all the others. Therefore, check if
                                            # the start is at midnight.
                                            if parsed_datetime.hour == 0 and parsed_datetime.minute == 0:
                                                # If it is midnight, great, store this as the first datetime
                                                first_date_time_in_directory = parsed_datetime
                                            else:
                                                # If it isn't, we need to skip on until we get to midnight
                                                continue

                                        # We want to output time as elapsed minutes, starting at zero
                                        # So, calculate the difference between this date and the first date found
                                        time_delta_from_start = parsed_datetime - first_date_time_in_directory
                                        # And then convert this time delta to number of seconds
                                        elapsed_seconds = time_delta_from_start.total_seconds()

                                        # Check if the solar radiation is negative. Some data sets have small negative
                                        # values (e.g. -1) at night, especially the Ny-Alesund dataset. This is due to
                                        # noise when the value is hovering around zero. To avoid problems later, make
                                        # zero. Also, check for ValueError - indicates garbage row or missing SWD value
                                        try:
                                            solar_radiation = int(row[swd_column_index])
                                        except ValueError:
                                            # print "Warning - found garbage row ({}) in file {}".format(row, file)
                                            data_quality_summary['Invalid solar radiation value'][file] += 1
                                            data_quality_log_writer.writerow(['Invalid solar radiation value', file] + [row])
                                            continue

                                        if solar_radiation < 0:
                                            # print "Warning - negative value {} found in directory {}".format(
                                            #     solar_radiation, sub_dir)
                                            data_quality_summary['Negative solar radiation value'][file] += 1
                                            data_quality_log_writer.writerow(['Negative solar radiation value', file] + [row])
                                            solar_radiation = 0

                                        # Output the elapsed minutes and solar radiation
                                        combined_writer.writerow([elapsed_seconds, solar_radiation])

                                        # Add the data to the graph data for plotting later
                                        graph_data[month_year_string]['x'].append(parsed_datetime)
                                        graph_data[month_year_string]['y'].append(solar_radiation)

                                if not found_header_row:
                                    sys.exit("Error - Didnt find header row containing Date/Time!")

                    # We have finished looping over all files in the subdirectory.
                    # Now we can plot graphs of all data in the subdirectory
                    # sub_dir is the station name, used for labelling graphs. sub_dir_path is the output directory.
                    # 1 refers to the granularity, which in this case is minutes
                    plot_monthly(graph_data, sub_dir, sub_dir_path, granularity=1)

    # Print data quality summary
    print "---Data quality summary---\n"
    for issue, files_and_counts in data_quality_summary.iteritems():
        print issue

        for issue_file, issue_count in files_and_counts.iteritems():
            print "\t{}: {}".format(issue_file, issue_count)
