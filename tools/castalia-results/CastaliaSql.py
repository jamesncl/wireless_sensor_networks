#!/usr/bin/env python3
import sys
from CastaliaResultParsing import parse_castalia
import sqlite3
from datetime import datetime
import time
import pandas


sqlite_database_filename = 'castalia-results.db'
sqlite_table_name = 'results'

map_location_to_climate = {
    'BER': 'Cfa',
    'BOU': 'BSk',
    'CAB': 'Cfb',
    'CAM': 'Cfb',
    'CLH': 'Cfb',
    'COC': 'Af',
    'GVN': 'EF',
    'LER': 'Cfb',
    'MAN': 'Af',
    'NYA': 'ET',
    'PAY': 'Cfb',
    'PSU': 'BSk',
    'REG': 'Dfb',
    'SBO': 'BSh',
    'SMS': 'Cfa',
    'SXF': 'Dfa',
    'TAM': 'BWh',
    'TAT': 'Cfa',
    'TOR': 'Dfb',
    'XIA': 'Dwa'
}


def load_from_sql_into_pandas_df(simpleOutputName, indexedOutputName):

    conn = sqlite3.connect(sqlite_database_filename)

    print("Loading from SQL")
    dataframe = pandas.read_sql_query(
        "SELECT * " 
        "FROM " + sqlite_table_name + " " +
        "WHERE simpleOutputName ='" + simpleOutputName + "' " +
        "AND indexedOutputName = '" + indexedOutputName + "'"
        , conn)
    conn.close()

    return dataframe


def load_from_folder_into_sql(infolder, filename, measures_to_load, solar_radiation_data_year=None, force_additional_sql_columns_created=None):
    """
        Parse castalia results files and load into a SQLite database
        Database columns:

        Location
        Granularity
        Day (if startSeconds parameter exists)
        Time (ditto)
        Repeat
        Node
        SimpleOutputName
        IndexedOutputName
        Value
        <ParamSweep1>
        <ParamSweep2>
        ...

    :param solar_radiation_data_year: to parse the dates / times in results files we need to know what year
                                      has been used as the solar radiation data fed into the simulation
    :param measures_to_load: list of tuples containin which result output names to load from file. Tuples in form:
                             (<simple_output_name>, <indexed_output_name>, <bool: include_sink_in_statistics?>)
    :param sqlite_table_name:
    :param sqlite_database_filename:
    :param filename: filename to look for, e.g. 'results.txt'
    :param infolder: a directory containing results files. May contain subdirectories. All subdirs will be searched.
    :return:
    """

    if solar_radiation_data_year is None:
        solar_radiation_data_year = 2007
        print("WARNING! - using default year for solar radiation - {}".format(solar_radiation_data_year))

    # results =
    #     {
    #           (label, repeat): {
    #                               module: {
    #                                          output_name: {
    #                                                          value_name: value(int)
    #                                                           ...
    #                                                       }
    #                                       }
    #                            }
    #     }
    #
    # example labels:
    # CtpRicer,General,varyLocation,varyCellSize,varyCapSize,varyMonth,traceFile=/vagrant/data/PANGEA/Lerwick/Lerwick-Combined-1-Minute.csv,cellSize=2,capSize=60,solarStart=-26380800
    #
    # Parse CtpRicer and anything with an '=' such as solarStart=-5212800,capSize=10,cellSize=2.5
    # Special case for traceFile: need to extract the location e.g. Lerwick from traceFile="/vagrant/data/PANGEA/Lerwick/Lerwick-Combined-1-Minute.csv
    #
    # output sql columns:
    # Location
    # if solarStart exists: Day, Time


    # Iterate over generator which yields results, 1 file at a time

    is_first_file = True
    suppress_distribution_warning = False

    for results, histograms in parse_castalia(infolder, filename, measures_to_load):

        # If this is the first file, setup the SQL table etc.
        if is_first_file:
            is_first_file = False

            # First we need to peek into the results and collect every param sweep name used
            # This is because we need to define a database column for each param name
            all_param_sweep_names = set()
            for label_and_repeat, modules in results.items():
                label = label_and_repeat[0]

                #  label will be a string containing e.g:
                #  MmbcrTmac,General,varyLocation,varyCellSize,varyCapSize,varyMonth,cellSize=4,solarStart=-2707200,capSize=40,traceFile=/vagrant/data/PANGEA/Tateno/Tateno-Combined-1-Minute.csv

                for label_token in label.split(','):
                    if '=' in label_token:
                        param_name = label_token.split('=', 1)[0]
                        all_param_sweep_names.add(param_name)

            print("Found params {}".format(all_param_sweep_names))

            conn = sqlite3.connect(sqlite_database_filename)
            c = conn.cursor()
            c.execute("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?", (sqlite_table_name,))
            table_exists = c.fetchone()[0]

            # If table exists
            if table_exists == 1:
                # Drop it
                print("Dropping table")
                c.execute("DROP TABLE {}".format(sqlite_table_name))

            # Create table

            # Create a list of column definitions for param sweep values
            param_sql_table_columns_def = ''
            for param_sweep_name in all_param_sweep_names:
                # Exclude traceFile as this is a long string which we will parse later into location column
                if param_sweep_name != "traceFile":
                    # add the column with appropriate data type
                    if param_sweep_name == "batchP":
                        param_sql_table_columns_def = param_sql_table_columns_def + ", {} TEXT".format(param_sweep_name)
                    else:
                        param_sql_table_columns_def = param_sql_table_columns_def + ", {} REAL".format(param_sweep_name)

            # Create a list of column definitions for additional columns if specified
            # These can be manually specified by the calling function, in case there is the scenario where
            # a parameter exists in some files but not others and therefore is not picked up when peeking in the first
            # results file
            if force_additional_sql_columns_created is not None:
                additional_table_columns_def = ''
                for force_additional_sql_column in force_additional_sql_columns_created:
                    # Check its not already in the param sweeps weve already picked up
                    if force_additional_sql_column[0] not in all_param_sweep_names:
                        additional_table_columns_def = \
                            additional_table_columns_def + ", {} {}".format(force_additional_sql_column[0],  # The column name
                                                                            force_additional_sql_column[1])  # The column type eg TEXT

            print("Creating table")
            sql_string = "CREATE TABLE " + sqlite_table_name + \
                         " (location TEXT, climate TEXT, protocol TEXT, loaddistribution TEXT, datetime INTEGER, monthnumber INTEGER, monthname TEXT, repeat INTEGER, node INTEGER, " + \
                         "simpleOutputName TEXT, indexedOutputName TEXT, value REAL " + \
                         param_sql_table_columns_def + \
                         additional_table_columns_def + \
                         ")"

            print(sql_string)
            c.execute(sql_string)

        for label_and_repeat, modules in results.items():
            #print(label_and_repeat)
            # label_and_repeat is a tuple of form (label, repeat)
            # e.g. ('Loc-Tam-Gra-30,Hours-12,startSeconds=-5212800', 4)

            # Split label_and_repeat tuple into separate variables
            label, repeat = label_and_repeat

            # Parse location
            # If location was varied, we extract it from traceFile=/vagrant/data/PANGEA/Tateno/Tateno-Combined-1-Minute.csv
            if 'traceFile=/vagrant/data/PANGEA/' in label:
                location = label.split('traceFile=/vagrant/data/PANGEA/')[1].split('/', 1)[0]
            # Else if location was static, extract it from the configuration name
            else:
                parsed_location_tokens = [t for t in label.split(',') if 'BER' in t
                                          or 'BOU' in t
                                          or 'CAB' in t
                                          or 'CAM' in t
                                          or 'COC' in t
                                          or 'CLH' in t
                                          or 'GVN' in t
                                          or 'LER' in t
                                          or 'MAN' in t
                                          or 'NYA' in t
                                          or 'PAY' in t
                                          or 'PSU' in t
                                          or 'REG' in t
                                          or 'SBO' in t
                                          or 'SMS' in t
                                          or 'SXF' in t
                                          or 'TAM' in t
                                          or 'TAT' in t
                                          or 'TOR' in t
                                          or 'XIA' in t]
                if len(parsed_location_tokens) != 1:
                    print("WARNING - couldn't parse location")
                    location = 'Unknown'
                else:
                    location = parsed_location_tokens[0]

            if location in map_location_to_climate:
                climate = map_location_to_climate[location]
            else:
                print("WARNING - couldn't find climate for location {}".format(location))
                climate = 'Unknown'

            # Parse CtpRicer, CtpBox, MmbcrRicer etc. from CtpRicer,General,varyLocation,varyCellSize,varyCapSize...
            # into protocols field
            if 'Ctp' in label or 'Mmbcr' in label or 'Static' in label:
                protocol_tokens = [t for t in label.split(',') if 'Ctp' in t or 'Mmbcr' in t or 'Static' in t]

                if len(protocol_tokens) != 1:
                    sys.exit("Unexpected count when parsing protocol {}".format(label))
                else:
                    protocol = protocol_tokens[0]
            else:
                print("WARNING - couldn't parse protocol")
                protocol = 'Unknown'
            # CtpRicer,General,varyLocation,varyCellSize,varyCapSize,varyMonth,traceFile=/vagrant/data/PANGEA/Lerwick/Lerwick-Combined-1-Minute.csv,cellSize=2,capSize=60,sol

            # Parse load distribution, e.g. loadUniform, loadPoisson etc.
            if 'load' in label:
                load_tokens = [t for t in label.split(',') if 'load' in t]

                if len(load_tokens) != 1:
                    sys.exit("Unexpected count when parsing load {}".format(label))
                else:
                    # Strip out the load substribng from, e.g. loadPoisson to leave just Poisson
                    load_distribution = load_tokens[0].replace('load', '')

                    # If its CompPoisson load, we need to append the p parameter to the load name
                    if load_distribution == 'CompPoisson':

                        # Find the batchP parameter in the label
                        batch_p_token = [t for t in label.split(',') if 'batchP=' in t]
                        if len(batch_p_token) != 1:
                            sys.exit("Unexpected count when parsing batchP param {}".format(label))
                        else:
                            # Extract the value from e.g. batchP=0.2
                            batch_p_value = batch_p_token[0].split('=')[1]
                            load_distribution = '{} {}'.format(load_distribution, batch_p_value)
            else:
                load_distribution = 'Uniform'

                if not suppress_distribution_warning:
                    print("WARNING - no load distribution config found so assuming Uniform (further warnings suppressed)")
                    suppress_distribution_warning = True

            # Extract any param=value pairs from label,  e.g.
            # Loc-Tam-Gra-30,Hours-12,startSeconds=-5212800,cellSize=2.5
            # has 2 param=values:
            # startSeconds=-5212800
            # cellSize=2.5
            # EXCLUDE traceFile= WHICH WE HAVE ALREADY PARSED
            param_sweep_dict = {}

            for label_token in label.split(','):
                if '=' in label_token and 'traceFile' not in label_token:
                    param_name, param_value = label_token.split('=', 1)
                    param_sweep_dict[param_name] = param_value

            # If we found a solarStart param name
            if 'solarStart' in param_sweep_dict:
                # define the UNIX timestamp associated with this
                # startSeconds is the number of seconds in the year (specified by solar_radiation_data_year)
                # So to convert to a UNIX timestamp (number of seconds since 1970) first get the UNIX
                # timestamp of the 1st Jan of base year
                start_unix_timestamp = time.mktime(datetime(solar_radiation_data_year, 1, 1, 0, 0, 0).timetuple())
                # then add the number of seconds
                # Note - startSeconds is a NEGATIVE, we must convert to POSITIVE using -(
                start_unix_timestamp += -(int(param_sweep_dict['solarStart']))
                start_datetime = datetime.fromtimestamp(start_unix_timestamp)
                month_number = start_datetime.month
                month_name = start_datetime.strftime("%b")

            else:
                start_unix_timestamp = 0
                month_number = 0
                month_name = "Unknown"

            for fully_qualified_module, output_names in modules.items():

                # fully_qualified_module is 'SN.node[0].ResourceManager.EnergySubsystem.EnergyStorage.Batteries[0]'
                # output_names is a dictionary of 'simple output name's

                # Extract the node id from the fully_qualified_module - it's between the first square brackets
                # e.g. SN.node[0].ResourceManager.EnergySubsystem.EnergyStorage.Batteries[0]
                # Note: find('[') returns ONLY THE FIRST square bracket found
                node = int(fully_qualified_module[fully_qualified_module.find('[') + 1:fully_qualified_module.find(']')])

                # Extract the module name from the source - it's the word after the last period
                module_name = fully_qualified_module.rsplit('.', 1)[-1]

                for simple_output_name, indexed_values in output_names.items():

                    # values is another dirctionary containing the 'indexed output name':value. Some simple output names
                    # have only 1 value, in which case the indexed output name is ''
                    #
                    # For example, if the reuslts file contained this:
                    #
                    # Castalia|	    simple output name:Disabled time %
                    # Castalia|			1.31352e-05
                    # Castalia|		simple output name:Energy breakdown
                    # Castalia|			0 Harvested
                    # Castalia|			1.08e+07 Initial
                    # Castalia|			0 Leaked
                    #
                    # output_name 'Disabled time %' will have a dictionary called 'values' which contains
                    # only 1 value with '' as the key:
                    #
                    # values = {
                    #               '': 1.31352e-05
                    #          }
                    #
                    # output_name 'Energy breakdown' will have a dictionary called 'values' which contains:
                    #
                    # values = {
                    #               'Harvested': 0,
                    #               'Initial': 1.08e+07,
                    #               'Leaked': 0
                    #           }

                    for indexed_output_name, output_value in indexed_values.items():

                        # Check the result names are in the list of tuples containing
                        # measures we want to save to SQL
                        # Tuples are in form (<simple_output_name>, <indexed_output_name>, <bool: include_sink_in_statistics?>)
                        #if (simple_output_name, indexed_output_name) in [(m[0], m[1]) for m in measures_to_load]:

                        sql_string = "INSERT INTO " + sqlite_table_name + \
                                     " (location, climate, protocol, loaddistribution, datetime, monthnumber, monthname, repeat, node, " + \
                                     "simpleOutputName, indexedOutputName, value"

                        for param_sweep_name in sorted(param_sweep_dict.keys()):
                            sql_string = sql_string + ", {}".format(param_sweep_name)

                        sql_string = sql_string + ") VALUES (" + \
                            "'{}', ".format(location) + \
                            "'{}', ".format(climate) + \
                            "'{}', ".format(protocol) + \
                            "'{}', ".format(load_distribution) + \
                            "{}, ".format(start_unix_timestamp) + \
                            "{}, ".format(month_number) + \
                            "'{}', ".format(month_name) + \
                            "{}, ".format(repeat) + \
                            "{}, ".format(node) + \
                            "'{}', ".format(simple_output_name) + \
                            "'{}', ".format(indexed_output_name) + \
                            "{}".format(output_value)

                        for param_sweep_name in sorted(param_sweep_dict.keys()):
                            sql_string = sql_string + ", {}".format(param_sweep_dict[param_sweep_name])

                        sql_string = sql_string + ")"
                        #print(sql_string)
                        c.execute(sql_string)

    conn.commit()
    conn.close()






