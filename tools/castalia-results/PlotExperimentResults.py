#!/usr/bin/env python3

import argparse
import math
import os
import sys
from datetime import datetime
from collections import defaultdict
import matplotlib.pyplot as plt
import plotly
import plotly.graph_objs as go
from plotly import tools
from CastaliaSql import load_from_sql_into_pandas_df
from CastaliaSql import load_from_folder_into_sql
import numpy
import scipy.stats


def parse_args():
    parser = argparse.ArgumentParser(description="Analyses Castalia results file")
    parser.add_argument('-i', '--infolder', type=str, required=False, default='./',
                        help="Path to folder containing results file(s). This folder and "
                             "all subfolders will be scanned. Multiple files found will be "
                             "combined. Defaults to current directory")
    parser.add_argument('-f', '--file', type=str, required=False, default='results.txt',
                        help="Trace filename to look for - defaults to results.txt")
    parser.add_argument('-c', '--cached', action='store_true', required=False, default=False,
                        help="If set, does not load data from results files - uses old data cached in SQL")
    parser.add_argument('-p', '--plottypes', type=str, required=False,
                        help="Output plot types to generate, e.g. line, scatter, line+scatter")

    return parser.parse_args()


def calculate_95_t_confidence_interval(values):
    return scipy.stats.t.interval(0.95,
                                  len(values)-1,
                                  loc=numpy.mean(values),
                                  scale=scipy.stats.sem(values))

def node_to_ring(node):

    if int(node) == 0:
        return 0
    elif int(node) in [6, 7, 10, 11]:
        return 1
    else:
        return 2


def ring_to_colour(ring):

    # https://matplotlib.org/examples/color/named_colors.html
    if ring == 0:
        return 'black'
    elif ring == 1:
        return 'green'
    else:
        return 'blue'


if __name__ == '__main__':
    args = parse_args()

    # Add trailing slash if required
    # if not args.infolder.endswith('/'):
    #     args.infolder = '{}/'.format(args.infolder)

    # (<simple_output_name>, <indexed_output_name>, <bool: include_sink_in_statistics?>, <str: plot_type>)

    # scatterindex means plot a node layout scatter diagram, using indexedOutputName as the nodeID
    # Note that scatterindex should probably contain '*' for indexed output name - this will mean that
    # all values are read
    # scatternode means plot a node layout scatter diagram, using node as the nodeID

    measures_to_plot = [
 ##       ('Application Avg latency', '', True, 'line'),
        #('Application Latency', '', True),
 ##       ('Application Received', '', True, 'line'),
 ##       ('Application Received From', '*', True, 'scatterindex'),
 ##       ('Application Send', '', False, 'scatternode+climatebar'),
        #('Application Send failed because out of energy', '', False),
        #('BoxMac CCA busy', '', False),
        #('BoxMac CCA clear', '', False),
        #('BoxMac Backoff congestion', '', False),
        #('BoxMac Overheard', '', False),
        #('BoxMac Backoff congestion', '', False),
        #('BoxMac Backoff initial', '', False),
        #('BoxMac Message train duration', '', False),
        #('BoxMac Messages in unicast train', '', False),
        #('BoxMac Idle listening', '', False),
        #('BoxMac Messages to send', '', False),
        #('BoxMac Msg not acked', '', False),
        #('BoxMac Overheard', '', False),
        #('BoxMac Received ACK', '', False),
        #('BoxMac Received broadcast', '', False),
        #('BoxMac Received data', '', False),
        #('BoxMac Sent ACK', '', False),
        #('BoxMac Sent broadcast', '', False),
        #('BoxMac Sent unicast', '', False),
        #('BoxMac Total sleep duration', '', False),
        #('Buffer overflow', '', False, 'line+scatternode'),  # This is declared in Radio and VirtualRouting!?!
 ##       ('CtpRouting Dropped packet after max retries', '', False, 'line+scatternode'),
        #('CtpRouting Dropped packet from buffer out of energy', '', False),
        #('CtpRouting received packet for forwarding', '', False, 'line'),
        #('CtpRouting hop count', '', False, 'line'),   # HISTOGRAM
        #('CtpRouting MH-ETX', '', False),              # HISTOGRAM
        #('CtpRouting SH-ETX to parent', '', False),    # HISTOGRAM
        #('CtpRouting Retrying send', '', False, 'line+scatternode'),
 ##       ('CtpRouting Times switched parent', '', False, 'line+scatternode'),
        #('Mmbcr retries', '', False),
        #('Mmbcr dropped packet after max retries', '', False),
        #('Mmbcr dropped packet from buffer out of energy', '', False),
        #('Mmbcr retrying send', '', False),
        #('Mmbcr received packet for forwarding', '', False),
        #('Mmbcr hop count', '', False),
        #('Device Energy', 'Charged', False),
        #('Device Energy', 'Drawn', False),
        #('Device Energy', 'Final', False),
        #('Device Energy', 'Initial', False),
        #('Device Energy', 'Maximum', False),
        #('Device Energy', 'Minimum', False),
 ##       ('Device Energy', 'Provided', False, 'line+scatternode'),
        #('Device Energy', 'Wasted', False),
 ##       ('Disabled time %', '', False, 'line+scatternode'),
 ##       ('Energy breakdown', 'Harvested', False, 'line'),
        #('Energy breakdown', 'Initial', False),
        #('Energy breakdown', 'Leaked', False),
        #('Energy breakdown', 'Maximum', False),
        #('Energy breakdown', 'Node switched off', False),
        #('Energy breakdown', 'Node switched on', False),
        #('Energy breakdown', 'Remaining', False),
        #('Energy breakdown', 'Supplied', False, 'line+scatternode'),
 ##       ('Energy breakdown', 'Wasted', False, 'line'),
        #('Estimated network lifetime (days)', '', False),
        #('LinkEstimator LQ', '', False),           # HISTOGRAM
        #('Packet bit lengths', '', False),
        #('RX pkt breakdown', 'Failed with NO interference', False),
        #('RX pkt breakdown', 'Failed with interference', False),
        #('RX pkt breakdown', 'Failed, below sensitivity', False),
        #('RX pkt breakdown', 'Failed, non RX state', False),
        #('RX pkt breakdown', 'Received despite interference', False),
        #('RX pkt breakdown', 'Received with NO interference', False),
        #('RX pkt for this node breakdown', 'Received with NO interference', False, 'line+scatternode'),
        #('RX pkt for this node breakdown', 'Received despite interference', False, 'line+scatternode'),
        ('RX pkt for this node breakdown', 'Failed with NO interference', False, 'line+scatternode'),
        ('RX pkt for this node breakdown', 'Failed with interference', False, 'line+scatternode'),
 ##       ('TXed pkts', 'TX pkts', False, 'line+scatternode')
    ]

    # Colours for line graphs
    map_protocols = {
        'MmbcrRicer': 'rgb(0, 61, 153)',
        'MmbcrBox': 'rgb(0, 82, 204)',
        'MmbcrTmac': 'rgb(153, 194, 255)',
        'CtpRicer': 'rgb(0, 102, 0)',
        'CtpBox': 'rgb(0, 61, 153)',
        'CtpTmac': 'rgb(128, 0, 0)',
        'StaticRicer': 'rgb(128, 0, 0)',
        'StaticBox': 'rgb(230, 0, 0)',
        'StaticTmac': 'rgb(255, 102, 102)'
    }

    # Colours for line graphs
    map_protocols_transparent = {
        'MmbcrRicer': 'rgba(0, 61, 153, 0.2)',
        'MmbcrBox': 'rgba(0, 82, 204, 0.2)',
        'MmbcrTmac': 'rgba(153, 194, 255, 0.2)',
        'CtpRicer': 'rgba((0, 102, 0, 0.2)',
        'CtpBox': 'rgba(0, 61, 153, 0.2)',
        'CtpTmac': 'rgba(128, 0, 0, 0.2)',
        'StaticRicer': 'rgba(128, 0, 0, 0.2)',
        'StaticBox': 'rgba(230, 0, 0, 0.2)',
        'StaticTmac': 'rgba(255, 102, 102, 0.2)'
    }

    # Colours for line graphs
    map_loads = {
        'Uniform': 'rgb(0, 61, 153)',
        'Poisson': 'rgb(0, 102, 0)',
        'CompPoisson 0.2': 'rgb(128, 0, 0)',
        'CompPoisson 0.3': 'rgb(230, 0, 0)',
        'CompPoisson 0.4': 'rgb(255, 102, 102)'
    }

    map_loads_transparent = {
        'Uniform': 'rgba(0, 61, 153, 0.2)',
        'Poisson': 'rgba(0, 102, 0, 0.2)',
        'CompPoisson 0.2': 'rgba(128, 0, 0, 0.2)',
        'CompPoisson 0.3': 'rgba(230, 0, 0, 0.2)',
        'CompPoisson 0.4': 'rgba(255, 102, 102, 0.2)'
    }

    # Set output plot size for saving graphs
    plt.rcParams["figure.figsize"] = (20, 10)

    # Get which results folder we are processing
    absolute_path = os.path.abspath(args.infolder)
    results_folder_name = absolute_path.split('/')[-1]
    print("Generating graphs for folder {}".format(results_folder_name))

    if results_folder_name == '2018-5-6':

        # Only some runs contain the batchP parameter.
        # Therefore we have to specify this as a forced additional SQL column,
        # because the script will (may) not find this column when peeking at the parameters
        # in the first results file
        if not args.cached:
            load_from_folder_into_sql(args.infolder, args.file, measures_to_plot, force_additional_sql_columns_created=[('batchP', 'TEXT')])

        for simple_output_name, indexed_output_name, include_sink_in_statistics, output_plot_types in measures_to_plot:

            print("Processing {} {}".format(simple_output_name, indexed_output_name))

            dataframe = load_from_sql_into_pandas_df(simple_output_name, indexed_output_name)

            # Get sorted list of unique dates / locations
            months = dataframe.monthnumber.unique()
            locations = dataframe.location.unique()
            load_distributions = dataframe.loaddistribution.unique()
            protocols = dataframe.protocol.unique()
            months.sort()
            locations.sort()
            load_distributions.sort()
            protocols.sort()

            if not include_sink_in_statistics:
                dataframe_subset = dataframe[(dataframe.node != 0)]
            else:
                dataframe_subset = dataframe

            if not output_plot_types:
                sys.exit("Error reading plot types for output")

            # Subset the data to just this output
            # If indexed_output_name is '*', meaning all values, then do not filter on it
            if indexed_output_name == '*':
                output_subset = dataframe_subset[(dataframe_subset.simpleOutputName == simple_output_name)]
            else:
                output_subset = dataframe_subset[(dataframe_subset.simpleOutputName == simple_output_name) &
                                                 (dataframe_subset.indexedOutputName == indexed_output_name)]

            # Find the max and min of all instances of this value
            # This is useful for normalising graphs - e.g. y axis range,
            # the colour of circle to plot etc
            max_value = output_subset['value'].max()
            min_value = output_subset['value'].min()

            if (args.plottypes is None or 'line' in args.plottypes) and 'line' in output_plot_types:
                ##############################################
                # Plot a line graph, 1 line for every protocol
                # Value on y axis, month on x axis
                # Subplots for each location
                # Split locations over multiple pages
                # Separate plots for each load distribution
                ##############################################

                # Plot this many locations on 1 page:
                locations_per_page = 4

                # Work out how many rows / cols on each page
                subplot_cols = 2
                subplot_rows = math.ceil(locations_per_page / subplot_cols)

                # How many pages:
                number_of_pages = int(math.ceil(len(locations) / locations_per_page))

                print("Line graphs: cols {} rows {} pages {}".format(subplot_cols, subplot_rows, number_of_pages))
                # Calculate the max value for this measure so we can set the max y axis to the same on all plots
                # (setting shared_yaxes=True on tools.make_subplots unfortunately only sets subplots on the same
                # row to share their y axes, so we need to do this manually)

                # max_y_value = dataframe_subset[(dataframe_subset.simpleOutputName == simple_output_name) &
                #                                     (dataframe_subset.indexedOutputName == indexed_output_name)].max()['value']

                for load_distribution in load_distributions:

                    output_load_distribution_subset = output_subset[(output_subset.loaddistribution == load_distribution)]

                    for page_number in range(number_of_pages):

                        # Work out, for this page, which locations are being plotted (for subplot titles)
                        first_location_index = 0 + (page_number * locations_per_page)
                        last_location_index = (locations_per_page - 1) + (page_number * locations_per_page)

                        fig = tools.make_subplots(
                            rows=subplot_rows,
                            cols=subplot_cols,
                            subplot_titles=(locations[first_location_index:last_location_index+1]),  # +1 because index slicing doesnt include the last index
                            shared_xaxes=True,
                            #shared_yaxes=True,  # only affects plots on same row
                            print_grid=False)  # Hides the annoying 'This is the format of your grid' console output

                        location_counter = 1
                        trace_counter = 0

                        print("Page {} locations {}-{} {}".format(
                            page_number,
                            first_location_index,
                            last_location_index,
                            locations[first_location_index:last_location_index+1]
                        ))

                        for location in locations[first_location_index:last_location_index+1]:

                            output_load_location_subset_grp_by_month_protocol = \
                                output_load_distribution_subset[(output_load_distribution_subset.location == location)] \
                                                                .groupby(['protocol', 'monthnumber'])

                            graph_dict = defaultdict(lambda: defaultdict(dict))

                            for grp_name, group in output_load_location_subset_grp_by_month_protocol:

                                # grp_name consists of the groupby columns specified above
                                protocol, month_number = grp_name

                                graph_dict[protocol][month_number]['mean'] = group['value'].mean()
                                graph_dict[protocol][month_number]['std'] = group['value'].std()

                            for protocol_key in sorted(graph_dict):

                                line_graph_y = [None] * 12
                                line_graph_std = [None] * 12

                                for month_key in sorted(graph_dict[protocol_key]):

                                    line_graph_y[month_key - 1] = graph_dict[protocol_key][month_key]['mean']
                                    line_graph_std[month_key - 1] = graph_dict[protocol_key][month_key]['std']

                                trace = go.Scatter(
                                    x=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
                                    y=line_graph_y,
                                    error_y=dict(
                                        type='data',
                                        array=line_graph_std,
                                        visible=True,
                                        color=map_protocols[protocol_key]
                                    ),
                                    line=dict(
                                        color=map_protocols[protocol_key],
                                        #width = 4
                                    ),
                                    mode='lines',
                                    name=protocol_key,
                                    legendgroup='a'
                                )


                                fig.append_trace(trace,
                                                 row=math.ceil(location_counter/2),
                                                 col=(2 - location_counter % 2))

                                fig['layout']['yaxis{}'.format(trace_counter+1)].update(range=[0, max_value])

                                trace_counter = trace_counter + 1

                            location_counter = location_counter + 1

                        # Don't try to plot if there was no data, this throws an exception
                        if trace_counter > 0:
                            fig['layout'].update(#height=600, width=600,
                                                 title="{} {} {} page {}".format(
                                                    simple_output_name,
                                                    indexed_output_name,
                                                    load_distribution,
                                                    page_number
                                                 ))

                            if not os.path.exists(os.path.join("GenericLineGraphs", "CompareProtocols")):
                                os.makedirs(os.path.join("GenericLineGraphs", "CompareProtocols"))

                            plotly.offline.plot(fig,
                                                filename=os.path.join(
                                                    "GenericLineGraphs",
                                                    "CompareProtocols",
                                                    "{}-{}-{}-{}.html".format(
                                                        simple_output_name,
                                                        indexed_output_name,
                                                        load_distribution,
                                                        page_number
                                                    )
                                                ),
                                                auto_open=False)
                        else:
                            print("No data")


                #################################################################################
                ##############################################
                # SAME AS ABOVE but instead of comparing
                # protocols on the same graph, with a separate
                # page for each load distribution,
                # compare load distributions on the same graph
                # with a separate page for each protocol
                ##############################################

                # Plot this many locations on 1 page:
                locations_per_page = 4

                # Work out how many rows / cols on each page
                subplot_cols = 2
                subplot_rows = math.ceil(locations_per_page / subplot_cols)

                # How many pages:
                number_of_pages = int(math.ceil(len(locations) / locations_per_page))

                for protocol in protocols:  #load_distribution in load_distributions:

                    output_protocol_subset = output_subset[(output_subset.protocol == protocol)]

                    for page_number in range(number_of_pages):

                        # Work out, for this page, which locations are being plotted (for subplot titles)
                        first_location_index = 0 + (page_number * locations_per_page)
                        last_location_index = (locations_per_page - 1) + (page_number * locations_per_page)

                        fig = tools.make_subplots(
                            rows=subplot_rows,
                            cols=subplot_cols,
                            subplot_titles=(locations[first_location_index:last_location_index+1]),  # +1 because index slicing doesnt include the last index
                            shared_xaxes=True,
                            #shared_yaxes=True,  # only affects plots on same row
                            print_grid=False)  # Hides the annoying 'This is the format of your grid' console output

                        location_counter = 1
                        trace_counter = 0

                        print("Page {} locations {}-{} {}".format(
                            page_number,
                            first_location_index,
                            last_location_index,
                            locations[first_location_index:last_location_index+1]
                        ))

                        for location in locations[first_location_index:last_location_index+1]:

                            output_protocol_location_subset_grp_by_month_load = \
                                output_protocol_subset[(output_protocol_subset.location == location)] \
                                                       .groupby(['loaddistribution', 'monthnumber'])

                            graph_dict = defaultdict(lambda: defaultdict(dict))

                            for grp_name, group in output_protocol_location_subset_grp_by_month_load:

                                # grp_name consists of the groupby columns specified above
                                load, month_number = grp_name

                                graph_dict[load][month_number]['mean'] = group['value'].mean()
                                graph_dict[load][month_number]['std'] = group['value'].std()

                            for load_key in sorted(graph_dict):

                                line_graph_y = [None] * 12
                                line_graph_std = [None] * 12

                                for month_key in sorted(graph_dict[load_key]):

                                    line_graph_y[month_key - 1] = graph_dict[load_key][month_key]['mean']
                                    line_graph_std[month_key - 1] = graph_dict[load_key][month_key]['std']

                                trace = go.Scatter(
                                    x=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
                                    y=line_graph_y,
                                    error_y=dict(
                                        type='data',
                                        array=line_graph_std,
                                        visible=True,
                                        color=map_loads[load_key]
                                    ),
                                    line=dict(
                                        color=map_loads[load_key],
                                        #width = 4
                                    ),
                                    mode='lines',
                                    name=load_key,
                                    legendgroup='a'
                                )

                                fig.append_trace(trace,
                                                 row=math.ceil(location_counter/2),
                                                 col=(2 - location_counter % 2))

                                fig['layout']['yaxis{}'.format(trace_counter+1)].update(range=[0, max_value])

                                trace_counter = trace_counter + 1

                            location_counter = location_counter + 1

                        # Don't try to plot if there was no data, this throws an exception
                        if trace_counter > 0:
                            fig['layout'].update(#height=600, width=600,
                                                 title="{} {} {} page {}".format(
                                                    simple_output_name,
                                                    indexed_output_name,
                                                    protocol,
                                                    page_number
                                                 ))

                            if not os.path.exists(os.path.join("GenericLineGraphs", "CompareLoads")):
                                os.makedirs(os.path.join("GenericLineGraphs", "CompareLoads"))

                            plotly.offline.plot(fig,
                                                filename=os.path.join(
                                                    "GenericLineGraphs",
                                                    "CompareLoads",
                                                    "{}-{}-{}-{}.html".format(
                                                        simple_output_name,
                                                        indexed_output_name,
                                                        protocol,
                                                        page_number
                                                    )
                                                ),
                                                auto_open=False)
                        else:
                            print("No data")

            if (args.plottypes is None or 'scatter' in args.plottypes) and 'scatter' in output_plot_types:

                ##################################################
                # Node layout chart, a bubble for each node,
                # coloured according to value
                # subplot for each month
                # 1 page for each parameter/location
                ##################################################

                map_node_to_scatter_location = {
                    0: (2.5, 2.5),
                    1: (0, 5),
                    2: (1, 5),
                    3: (2, 5),
                    4: (3, 5),
                    5: (4, 5),
                    6: (5, 5),
                    7: (0, 4),
                    8: (1, 4),
                    9: (2, 4),
                    10: (3, 4),
                    11: (4, 4),
                    12: (5, 4),
                    13: (0, 3),
                    14: (1, 3),
                    15: (2, 3),
                    16: (3, 3),
                    17: (4, 3),
                    18: (5, 3),
                    19: (0, 2),
                    20: (1, 2),
                    21: (2, 2),
                    22: (3, 2),
                    23: (4, 2),
                    24: (5, 2),
                    25: (0, 1),
                    26: (1, 1),
                    27: (2, 1),
                    28: (3, 1),
                    29: (4, 1),
                    30: (5, 1),
                    31: (0, 0),
                    32: (1, 0),
                    33: (2, 0),
                    34: (3, 0),
                    35: (4, 0),
                    36: (5, 0)
                }

                print("Node scatter graphs")

                for location in locations:

                    # Subset of just his location
                    output_location_subset = output_subset[(output_subset.location == location)]

                    for protocol in sorted(map_protocols):

                        # Subset to just this location
                        output_location_protocol_subset = output_location_subset[(output_location_subset.protocol == protocol)]

                        for load_distribution in load_distributions:

                            # Subset to just this load distribution
                            output_location_protocol_load_subset = \
                                output_location_protocol_subset[(output_location_protocol_subset.loaddistribution == load_distribution)]

                            # Output a page containing every month for a specific output, location, protocol
                            # So, create subplots grid, 1 for each month:

                            fig = tools.make_subplots(
                                rows=3,
                                cols=4,
                                subplot_titles=(['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']),
                                #shared_xaxes=True,
                                #shared_yaxes=True,  # only affects plots on same row
                                print_grid=False,  # Hides the annoying 'This is the format of your grid' console output
                                #specs=[[{}, {}, {}, {}],[{}, {}, {}, {}],[{}, {}, {}, {}]],  # Helps make the subplot margins small according to https://stackoverflow.com/questions/31526045/remove-space-between-subplots-in-plotly
                                horizontal_spacing=0.1,
                                vertical_spacing=0.1
                            )

                            found_data = False
                            #trace_number = 1

                            for month_index in range(12):

                                # +1 because month_index is zero-based
                                month_number = month_index + 1

                                # subset down to the specific month
                                output_location_protocol_load_month_subset = \
                                    output_location_protocol_load_subset[(output_location_protocol_load_subset.monthnumber == month_number)]

                                # Group by node
                                # Depending on the output type, the node value we are interested in is either the
                                # node reporting the value, or the indexedOutputName which indicates the node the value
                                # is referring to (e.g. for 'Application Received From', the reporting node is the
                                # sink (node 0) but we are interested in the node it is reporting about (indexedOutputName)
                                if 'scatterindex' in output_plot_types:
                                    grp_by_node = output_location_protocol_load_month_subset.groupby(['indexedOutputName'])
                                else:
                                    grp_by_node = output_location_protocol_load_month_subset.groupby(['node'])

                                trace_data_x = []
                                trace_data_y = []
                                trace_data_colour = []

                                for node, group in grp_by_node:

                                    mean_value = group['value'].mean()

                                    #sys.exit()
                                    # Normalise the colour value over the min/max of all values so we get a real 0-1
                                    trace_data_colour.append((mean_value - min_value) / (max_value / min_value))
                                    trace_data_x.append(map_node_to_scatter_location[int(node)][0])
                                    trace_data_y.append(map_node_to_scatter_location[int(node)][1])

                                    found_data = True

                                trace = go.Scatter(
                                    x=trace_data_x,
                                    y=trace_data_y,
                                    mode='markers',
                                    marker=dict(size=14,
                                                line=dict(width=1),
                                                color=trace_data_colour,
                                                #opacity= 0.3
                                                )
                                    #name=protocol_key,
                                    #legendgroup='a'
                                )

                                fig.append_trace(trace,
                                                 row=math.ceil(month_number/4),  # 4 because 4 columns
                                                 col=(month_index % 4) + 1)

                                #fig['layout']['height{}'.format(trace_number)].update(300)
                                #fig['layout']['width{}'.format(trace_number)].update(300)

                                #trace_number = trace_number + 1

                            # Don't attempt to plot if no data, will throw exception
                            if found_data:
                                fig['layout'].update(#height=600, width=600,
                                                     title="{} {} {} {} {}".format(
                                                         simple_output_name,
                                                         indexed_output_name,
                                                         location,
                                                         protocol,
                                                         load_distribution
                                                     ))

                                if not os.path.exists("GenericNodeLayout"):
                                    os.mkdir("GenericNodeLayout")

                                plotly.offline.plot(fig,
                                                    filename=os.path.join("GenericNodeLayout",
                                                                          "{}-{}-{}-{}-{}.html".format(
                                                                              simple_output_name,
                                                                              indexed_output_name,
                                                                              location,
                                                                              protocol,
                                                                              load_distribution
                                                                           )),
                                                    auto_open=False)

                            else:
                                print("No data for {} {}".format(location, protocol))

            # if (args.plottypes is None or 'climatebar' in args.plottypes) and 'climatebar' in output_plot_types:
            #     ##############################################
            #     # Plot a bar graph, 1 bar for every climate
            #     ##############################################
            #
            #     # Work out how many loads / protocols there are
            #     # so we know how many subplots to generate
            #     number_of_loads_protocols = output_subset.groupby(['loaddistribution','protocol']).size()
            #     print(number_of_loads_protocols)
            #     sys.exit()
            #
            #     for load_distribution in load_distributions:
            #
            #         output_load_subset = output_subset[(output_subset.loaddistribution == load_distribution)]
            #
            #         for protocol in sorted(map_protocols):
            #
            #             # Subset to just this location
            #             output_load_protocol_subset = output_load_subset[(output_load_subset.protocol == protocol)]



    elif results_folder_name == '2018-5-23':

        if not args.cached:
            load_from_folder_into_sql(args.infolder, args.file, measures_to_plot)

        for simple_output_name, indexed_output_name, include_sink_in_statistics, output_plot_types in measures_to_plot:

            if simple_output_name == 'Application Received':

                print("Processing {} {}".format(simple_output_name, indexed_output_name))

                dataframe = load_from_sql_into_pandas_df(simple_output_name, indexed_output_name)

                if not include_sink_in_statistics:
                    dataframe_subset = dataframe[(dataframe.node != 0)]
                else:
                    dataframe_subset = dataframe

                # Get sorted list of unique dates / locations
                dates = dataframe.datetime.unique()
                locations = dataframe.location.unique()
                protocols = dataframe.protocol.unique()
                dates.sort()
                locations.sort()
                protocols.sort()

                max_value = dataframe['value'].max()

                for location in locations:

                    for protocol in protocols:

                        ############### 1 PLOT, ALL DAYS AVERAGED ######################


                        # location, granularity, datetime, repeat, node, simpleOutputName, indexedOutputName, value, cellSize, capSize

                        location_protocol_subset = dataframe_subset[(dataframe_subset.location == location) &
                                                           (dataframe_subset.protocol == protocol)]

                        pivot = location_protocol_subset.pivot_table(index='cellSize',
                             columns='capSize',
                             values='value',
                             aggfunc='mean')

                        pivot.sort_index(level=['cellSize', 'capSize'], ascending=True)

                        surface_data = go.Surface(
                                x=pivot.columns.values.tolist(),
                                y=pivot.index.values.tolist(),
                                z=pivot.values,
                                colorscale='Viridis',
                                contours=dict(
                                    # x=dict(
                                    #     show=True,
                                    #     color='rgb(0,0,0)'
                                    #     #project=dict(x=True)
                                    # ),
                                    y=dict(
                                        show=True,
                                        color='rgb(255,255,255)'
                                        #project=dict(y=True)
                                    ),
                                    z=dict(
                                        show=True,
                                        color='rgb(255,255,255)'
                                        #project=dict(y=True)
                                    )
                                )

                            )

                        data = [surface_data]

                        if simple_output_name == "Application Received":
                            layout = go.Layout(

                                scene=dict(
                                    xaxis=dict(
                                        title='Capacitor size (F)',
                                        titlefont=dict(
                                            #family='Courier New, monospace',
                                            size=26,
                                        )
                                    ),
                                    yaxis=dict(
                                        title='Solar cell size (cm^2)',
                                        titlefont=dict(
                                            #family='Courier New, monospace',
                                            size=26,
                                        )
                                    ),
                                    zaxis=dict(
                                        title='{} {}'.format("Delivered packets" if simple_output_name == "Application Received" else simple_output_name, indexed_output_name),
                                        titlefont=dict(
                                            #family='Courier New, monospace',
                                            size=26,
                                        ),
                                        range=[0, max_value]
                                    )
                                )
                            )
                        # else:
                        #     layout = go.Layout(
                        #
                        #         scene=dict(
                        #             xaxis=dict(
                        #                 title='Capacitor size (F)',
                        #                 titlefont=dict(
                        #                     #family='Courier New, monospace',
                        #                     size=26,
                        #                 )
                        #             ),
                        #             yaxis=dict(
                        #                 title='Solar cell size (cm^2)',
                        #                 titlefont=dict(
                        #                     #family='Courier New, monospace',
                        #                     size=26,
                        #                 )
                        #             ),
                        #             zaxis=dict(
                        #                 title='{} {}'.format("Delivered packets" if simple_output_name == "Application Received" else simple_output_name, indexed_output_name),
                        #                 titlefont=dict(
                        #                     #family='Courier New, monospace',
                        #                     size=26,
                        #                 )
                        #             )
                        #         )
                        #     )
                        #
                        #     #width=700,
                        #     # margin=dict(
                        #     #     r=20, b=10,
                        #     #     l=10, t=10
                        #     # )

                        filename = os.path.join(args.infolder,
                                                '{}-{}-{}-3D-cell-vs-cap.html'.format(
                                                    '{} {}'.format(simple_output_name, indexed_output_name),
                                                    location,
                                                    protocol
                                                ))
                        fig = go.Figure(data=data, layout=layout)
                        plotly.offline.plot(fig, filename=filename, auto_open=False)
                        print("Saved {}".format(filename))

    if results_folder_name == '2018-5-24':

        # SAME AS 2018-5-24 BUT WITH DAYS INSTEAD OF MONTHS
        if not args.cached:
            load_from_folder_into_sql(args.infolder, args.file, measures_to_plot, force_additional_sql_columns_created=[('batchP', 'TEXT')])

        for simple_output_name, indexed_output_name, include_sink_in_statistics, output_plot_types in measures_to_plot:

            print("Processing {} {}".format(simple_output_name, indexed_output_name))

            dataframe = load_from_sql_into_pandas_df(simple_output_name, indexed_output_name)

            # Get sorted list of unique dates / locations
            days = dataframe.datetime.unique()
            locations = dataframe.location.unique()
            load_distributions = dataframe.loaddistribution.unique()
            protocols = dataframe.protocol.unique()
            days.sort()
            locations.sort()
            load_distributions.sort()
            protocols.sort()

            days_formatted = [datetime.fromtimestamp(d) for d in days]

            if not include_sink_in_statistics:
                dataframe_subset = dataframe[(dataframe.node != 0)]
            else:
                dataframe_subset = dataframe

            if not output_plot_types:
                sys.exit("Error reading plot types for output")

            # Subset the data to just this output
            # If indexed_output_name is '*', meaning all values, then do not filter on it
            if indexed_output_name == '*':
                output_subset = dataframe_subset[(dataframe_subset.simpleOutputName == simple_output_name)]
            else:
                output_subset = dataframe_subset[(dataframe_subset.simpleOutputName == simple_output_name) &
                                                 (dataframe_subset.indexedOutputName == indexed_output_name)]

            # Find the max and min of all instances of this value
            # This is useful for normalising graphs - e.g. y axis range,
            # the colour of circle to plot etc
            max_value = output_subset['value'].max()
            min_value = output_subset['value'].min()

            if (args.plottypes is None or 'line' in args.plottypes) and 'line' in output_plot_types:
                ##############################################
                # Plot a line graph, 1 line for every protocol
                # Value on y axis, DAY on x axis
                # Subplots for each location
                # Split locations over multiple pages
                # Separate plots for each load distribution
                ##############################################

                # Plot this many locations on 1 page:
                locations_per_page = 4

                # Work out how many rows / cols on each page
                subplot_cols = 2
                subplot_rows = math.ceil(locations_per_page / subplot_cols)

                # How many pages:
                number_of_pages = int(math.ceil(len(locations) / locations_per_page))

                print("Line graphs: cols {} rows {} pages {}".format(subplot_cols, subplot_rows, number_of_pages))
                # Calculate the max value for this measure so we can set the max y axis to the same on all plots
                # (setting shared_yaxes=True on tools.make_subplots unfortunately only sets subplots on the same
                # row to share their y axes, so we need to do this manually)

                # max_y_value = dataframe_subset[(dataframe_subset.simpleOutputName == simple_output_name) &
                #                                     (dataframe_subset.indexedOutputName == indexed_output_name)].max()['value']

                for load_distribution in load_distributions:

                    output_load_distribution_subset = output_subset[(output_subset.loaddistribution == load_distribution)]

                    for page_number in range(number_of_pages):

                        # Work out, for this page, which locations are being plotted (for subplot titles)
                        first_location_index = 0 + (page_number * locations_per_page)
                        last_location_index = (locations_per_page - 1) + (page_number * locations_per_page)

                        # We need two figures, one for the normal plot style (line plus standard error bars)
                        # and one for the continuous confidence interval style
                        fig_standard_plot = tools.make_subplots(
                            rows=subplot_rows,
                            cols=subplot_cols,
                            subplot_titles=(locations[first_location_index:last_location_index+1]),  # +1 because index slicing doesnt include the last index
                            shared_xaxes=True,
                            #shared_yaxes=True,  # only affects plots on same row
                            print_grid=False)  # Hides the annoying 'This is the format of your grid' console output

                        fig_continuous_error_plot = tools.make_subplots(
                            rows=subplot_rows,
                            cols=subplot_cols,
                            subplot_titles=(locations[first_location_index:last_location_index+1]),  # +1 because index slicing doesnt include the last index
                            shared_xaxes=True,
                            #shared_yaxes=True,  # only affects plots on same row
                            print_grid=False)  # Hides the annoying 'This is the format of your grid' console output

                        location_counter = 1
                        trace_counter = 0

                        print("Page {} locations {}-{} {}".format(
                            page_number,
                            first_location_index,
                            last_location_index,
                            locations[first_location_index:last_location_index+1]
                        ))

                        for location in locations[first_location_index:last_location_index+1]:

                            output_load_location_subset_grp_by_day_protocol = \
                                output_load_distribution_subset[(output_load_distribution_subset.location == location)] \
                                                                .groupby(['protocol', 'datetime'])

                            graph_dict = defaultdict(lambda: defaultdict(dict))

                            for grp_name, group in output_load_location_subset_grp_by_day_protocol:

                                # grp_name consists of the groupby columns specified above
                                protocol, day_number = grp_name

                                graph_dict[protocol][day_number]['mean'] = group['value'].mean()
                                graph_dict[protocol][day_number]['std'] = group['value'].std()

                                # Confidence interval
                                group_values = group['value'].tolist()
                                confidence_interval = calculate_95_t_confidence_interval(group_values)
                                graph_dict[protocol][day_number]['ci_lower'] = confidence_interval[0]
                                graph_dict[protocol][day_number]['ci_upper'] = confidence_interval[1]

                            for protocol_key in sorted(graph_dict):

                                line_graph_y = []
                                line_graph_ci_upper = []
                                line_graph_ci_lower = []
                                line_graph_std = []

                                for day_key in sorted(graph_dict[protocol_key]):

                                    line_graph_y.append(graph_dict[protocol_key][day_key]['mean'])
                                    line_graph_std.append(graph_dict[protocol_key][day_key]['std'])
                                    line_graph_ci_lower.append(graph_dict[protocol_key][day_key]['ci_lower'])
                                    line_graph_ci_upper.append(graph_dict[protocol_key][day_key]['ci_upper'])

                                # We have to do some wierdness here to get the continuous CI bands to work - see https://plot.ly/python/continuous-error-bars/
                                x_reversed = days_formatted[::-1]  # [::-1] reverses the order
                                line_graph_ci_lower = line_graph_ci_lower[::-1]

                                # STANDARD plot
                                fig_standard_plot.append_trace(go.Scatter(
                                                                    x=days_formatted,  # + x_reversed is part of getting the ci bands to work
                                                                    y=line_graph_y,
                                                                    error_y=dict(
                                                                        type='data',
                                                                        array=line_graph_std,
                                                                        visible=True,
                                                                        color=map_protocols[protocol_key]
                                                                    ),
                                                                    line=dict(
                                                                        color=map_protocols[protocol_key],
                                                                        #width = 4
                                                                    ),
                                                                    mode='lines',
                                                                    name=protocol_key,
                                                                ),
                                                                row=math.ceil(location_counter/2),
                                                                col=(2 - location_counter % 2))

                                # CONTINUOUS ERROR plot - one for the shaded error area
                                fig_continuous_error_plot.append_trace(go.Scatter(
                                                                    x=days_formatted + x_reversed,  # + x_reversed is part of getting the ci bands to work
                                                                    y=line_graph_ci_upper + line_graph_ci_lower,  # line_graph_y,
                                                                    fill='tozerox',
                                                                    fillcolor=map_protocols_transparent[protocol_key],
                                                                    line=dict(
                                                                        color='transparent', #color=map_protocols[protocol_key],
                                                                        #width = 4
                                                                    ),
                                                                    name=protocol_key,
                                                                ),
                                                                row=math.ceil(location_counter/2),
                                                                col=(2 - location_counter % 2))

                                # ... and another for the actual line
                                fig_continuous_error_plot.append_trace(go.Scatter(
                                                                    x=days_formatted,
                                                                    y=line_graph_y,
                                                                    line=dict(
                                                                        color=map_protocols[protocol_key],
                                                                        #width = 4
                                                                    ),
                                                                    name=protocol_key,
                                                                ),
                                                                row=math.ceil(location_counter/2),
                                                                col=(2 - location_counter % 2))

                                # For date formats see https://github.com/d3/d3-time-format/blob/master/README.md#locale_format
                                fig_standard_plot['layout']['yaxis{}'.format(trace_counter+1)].update(range=[0, (max_value if simple_output_name != 'RX pkt for this node breakdown' else 200000)])
                                fig_standard_plot['layout']['xaxis{}'.format(trace_counter+1)].update(tickformat='%B')
                                fig_continuous_error_plot['layout']['yaxis{}'.format(trace_counter+1)].update(range=[0, (max_value if simple_output_name != 'RX pkt for this node breakdown' else 200000)])
                                fig_continuous_error_plot['layout']['xaxis{}'.format(trace_counter+1)].update(tickformat='%B')

                                trace_counter = trace_counter + 1

                            location_counter = location_counter + 1

                        # Don't try to plot if there was no data, this throws an exception
                        if trace_counter > 0:
                            fig_standard_plot['layout'].update(#height=600, width=600,
                                                 title="{} {} {} page {}".format(
                                                    simple_output_name,
                                                    indexed_output_name,
                                                    load_distribution,
                                                    page_number
                                                 ))

                            fig_continuous_error_plot['layout'].update(#height=600, width=600,
                                                 title="{} {} {} page {}".format(
                                                    simple_output_name,
                                                    indexed_output_name,
                                                    load_distribution,
                                                    page_number
                                                 ))

                            if not os.path.exists(os.path.join("GenericLineGraphs", "CompareProtocols")):
                                os.makedirs(os.path.join("GenericLineGraphs", "CompareProtocols"))

                            plotly.offline.plot(fig_standard_plot,
                                                filename=os.path.join(
                                                    "GenericLineGraphs",
                                                    "CompareProtocols",
                                                    "standard-{}-{}-{}-{}.html".format(
                                                        simple_output_name,
                                                        indexed_output_name,
                                                        load_distribution,
                                                        page_number
                                                    )
                                                ),
                                                auto_open=False)

                            plotly.offline.plot(fig_continuous_error_plot,
                                                filename=os.path.join(
                                                    "GenericLineGraphs",
                                                    "CompareProtocols",
                                                    "continuous-{}-{}-{}-{}.html".format(
                                                        simple_output_name,
                                                        indexed_output_name,
                                                        load_distribution,
                                                        page_number
                                                    )
                                                ),
                                                auto_open=False)


                        else:
                            print("No data")


                #################################################################################
                ##############################################
                # SAME AS ABOVE but instead of comparing
                # protocols on the same graph, with a separate
                # page for each load distribution,
                # compare load distributions on the same graph
                # with a separate page for each protocol
                ##############################################

                # Plot this many locations on 1 page:
                locations_per_page = 4

                # Work out how many rows / cols on each page
                subplot_cols = 2
                subplot_rows = math.ceil(locations_per_page / subplot_cols)

                # How many pages:
                number_of_pages = int(math.ceil(len(locations) / locations_per_page))

                for protocol in protocols:  #load_distribution in load_distributions:

                    output_protocol_subset = output_subset[(output_subset.protocol == protocol)]

                    for page_number in range(number_of_pages):

                        # Work out, for this page, which locations are being plotted (for subplot titles)
                        first_location_index = 0 + (page_number * locations_per_page)
                        last_location_index = (locations_per_page - 1) + (page_number * locations_per_page)

                        fig_standard_plot = tools.make_subplots(
                            rows=subplot_rows,
                            cols=subplot_cols,
                            subplot_titles=(locations[first_location_index:last_location_index+1]),  # +1 because index slicing doesnt include the last index
                            shared_xaxes=True,
                            #shared_yaxes=True,  # only affects plots on same row
                            print_grid=False)  # Hides the annoying 'This is the format of your grid' console output

                        fig_continuous_error_plot = tools.make_subplots(
                            rows=subplot_rows,
                            cols=subplot_cols,
                            subplot_titles=(locations[first_location_index:last_location_index+1]),  # +1 because index slicing doesnt include the last index
                            shared_xaxes=True,
                            #shared_yaxes=True,  # only affects plots on same row
                            print_grid=False)  # Hides the annoying 'This is the format of your grid' console output

                        location_counter = 1
                        trace_counter = 0

                        print("Page {} locations {}-{} {}".format(
                            page_number,
                            first_location_index,
                            last_location_index,
                            locations[first_location_index:last_location_index+1]
                        ))

                        for location in locations[first_location_index:last_location_index+1]:

                            output_protocol_location_subset_grp_by_day_load = \
                                output_protocol_subset[(output_protocol_subset.location == location)] \
                                                       .groupby(['loaddistribution', 'datetime'])

                            graph_dict = defaultdict(lambda: defaultdict(dict))

                            for grp_name, group in output_protocol_location_subset_grp_by_day_load:

                                # grp_name consists of the groupby columns specified above
                                load, day_number = grp_name

                                graph_dict[load][day_number]['mean'] = group['value'].mean()
                                graph_dict[load][day_number]['std'] = group['value'].std()

                                # Confidence interval
                                group_values = group['value'].tolist()
                                confidence_interval = calculate_95_t_confidence_interval(group_values)
                                graph_dict[load][day_number]['ci_lower'] = confidence_interval[0]
                                graph_dict[load][day_number]['ci_upper'] = confidence_interval[1]


                            for load_key in sorted(graph_dict):

                                line_graph_y = []
                                line_graph_ci_upper = []
                                line_graph_ci_lower = []
                                line_graph_std = []

                                for day_key in sorted(graph_dict[load_key]):

                                    line_graph_y.append(graph_dict[load_key][day_key]['mean'])
                                    line_graph_std.append(graph_dict[load_key][day_key]['std'])
                                    line_graph_ci_lower.append(graph_dict[load_key][day_key]['ci_lower'])
                                    line_graph_ci_upper.append(graph_dict[load_key][day_key]['ci_upper'])

                                # We have to do some wierdness here to get the continuous CI bands to work - see https://plot.ly/python/continuous-error-bars/
                                x_reversed = days_formatted[::-1]  # [::-1] reverses the order
                                line_graph_ci_lower = line_graph_ci_lower[::-1]

                                # STANDARD plot
                                fig_standard_plot.append_trace(go.Scatter(
                                                                    x=days_formatted,  # + x_reversed is part of getting the ci bands to work
                                                                    y=line_graph_y,
                                                                    error_y=dict(
                                                                        type='data',
                                                                        array=line_graph_std,
                                                                        visible=True,
                                                                        color=map_loads[load_key]
                                                                    ),
                                                                    line=dict(
                                                                        color=map_loads[load_key],
                                                                        #width = 4
                                                                    ),
                                                                    mode='lines',
                                                                    name=load_key,
                                                                ),
                                                                row=math.ceil(location_counter/2),
                                                                col=(2 - location_counter % 2))

                                # CONTINUOUS ERROR plot - one for the shaded error area
                                fig_continuous_error_plot.append_trace(go.Scatter(
                                                                    x=days_formatted + x_reversed,  # + x_reversed is part of getting the ci bands to work
                                                                    y=line_graph_ci_upper + line_graph_ci_lower,  # line_graph_y,
                                                                    fill='tozerox',
                                                                    fillcolor=map_loads_transparent[load_key],
                                                                    line=dict(
                                                                        color='transparent', #color=map_protocols[protocol_key],
                                                                        #width = 4
                                                                    ),
                                                                    name=load_key,
                                                                ),
                                                                row=math.ceil(location_counter/2),
                                                                col=(2 - location_counter % 2))

                                # ... and another for the actual line
                                fig_continuous_error_plot.append_trace(go.Scatter(
                                                                    x=days_formatted,
                                                                    y=line_graph_y,
                                                                    line=dict(
                                                                        color=map_loads[load_key],
                                                                        #width = 4
                                                                    ),
                                                                    name=load_key,
                                                                ),
                                                                row=math.ceil(location_counter/2),
                                                                col=(2 - location_counter % 2))

                                # For date formats see https://github.com/d3/d3-time-format/blob/master/README.md#locale_format
                                fig_standard_plot['layout']['yaxis{}'.format(trace_counter+1)].update(range=[0, (max_value if simple_output_name != 'RX pkt for this node breakdown' else 200000)])
                                fig_standard_plot['layout']['xaxis{}'.format(trace_counter+1)].update(tickformat='%B')
                                fig_continuous_error_plot['layout']['yaxis{}'.format(trace_counter+1)].update(range=[0, (max_value if simple_output_name != 'RX pkt for this node breakdown' else 200000)])
                                fig_continuous_error_plot['layout']['xaxis{}'.format(trace_counter+1)].update(tickformat='%B')

                                trace_counter = trace_counter + 1

                            location_counter = location_counter + 1

                        # Don't try to plot if there was no data, this throws an exception
                        if trace_counter > 0:
                            fig_standard_plot['layout'].update(#height=600, width=600,
                                                 title="{} {} {} page {}".format(
                                                    simple_output_name,
                                                    indexed_output_name,
                                                    protocol,
                                                    page_number
                                                 ))

                            fig_continuous_error_plot['layout'].update(#height=600, width=600,
                                                 title="{} {} {} page {}".format(
                                                    simple_output_name,
                                                    indexed_output_name,
                                                    protocol,
                                                    page_number
                                                 ))

                            if not os.path.exists(os.path.join("GenericLineGraphs", "CompareLoads")):
                                os.makedirs(os.path.join("GenericLineGraphs", "CompareLoads"))

                            plotly.offline.plot(fig_standard_plot,
                                                filename=os.path.join(
                                                    "GenericLineGraphs",
                                                    "CompareLoads",
                                                    "standard-{}-{}-{}-{}.html".format(
                                                        simple_output_name,
                                                        indexed_output_name,
                                                        protocol,
                                                        page_number
                                                    )
                                                ),
                                                auto_open=False)

                            plotly.offline.plot(fig_continuous_error_plot,
                                                filename=os.path.join(
                                                    "GenericLineGraphs",
                                                    "CompareLoads",
                                                    "continuous-{}-{}-{}-{}.html".format(
                                                        simple_output_name,
                                                        indexed_output_name,
                                                        protocol,
                                                        page_number
                                                    )
                                                ),
                                                auto_open=False)
                        else:
                            print("No data")

            if (args.plottypes is None or 'scatter' in args.plottypes) and 'scatter' in output_plot_types:

                ##################################################
                # Node layout chart, a bubble for each node,
                # coloured according to value
                # subplot for each month
                # 1 page for each parameter/location
                ##################################################

                map_node_to_scatter_location = {
                    0: (2.5, 2.5),
                    1: (0, 5),
                    2: (1, 5),
                    3: (2, 5),
                    4: (3, 5),
                    5: (4, 5),
                    6: (5, 5),
                    7: (0, 4),
                    8: (1, 4),
                    9: (2, 4),
                    10: (3, 4),
                    11: (4, 4),
                    12: (5, 4),
                    13: (0, 3),
                    14: (1, 3),
                    15: (2, 3),
                    16: (3, 3),
                    17: (4, 3),
                    18: (5, 3),
                    19: (0, 2),
                    20: (1, 2),
                    21: (2, 2),
                    22: (3, 2),
                    23: (4, 2),
                    24: (5, 2),
                    25: (0, 1),
                    26: (1, 1),
                    27: (2, 1),
                    28: (3, 1),
                    29: (4, 1),
                    30: (5, 1),
                    31: (0, 0),
                    32: (1, 0),
                    33: (2, 0),
                    34: (3, 0),
                    35: (4, 0),
                    36: (5, 0)
                }

                print("Node scatter graphs")

                for location in locations:

                    # Subset of just his location
                    output_location_subset = output_subset[(output_subset.location == location)]

                    for protocol in sorted(map_protocols):

                        # Subset to just this location
                        output_location_protocol_subset = output_location_subset[(output_location_subset.protocol == protocol)]

                        for load_distribution in load_distributions:

                            # Subset to just this load distribution
                            output_location_protocol_load_subset = \
                                output_location_protocol_subset[(output_location_protocol_subset.loaddistribution == load_distribution)]

                            # Output a page containing every month for a specific output, location, protocol
                            # So, create subplots grid, 1 for each month:

                            fig = tools.make_subplots(
                                rows=3,
                                cols=4,
                                subplot_titles=(['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']),
                                #shared_xaxes=True,
                                #shared_yaxes=True,  # only affects plots on same row
                                print_grid=False,  # Hides the annoying 'This is the format of your grid' console output
                                #specs=[[{}, {}, {}, {}],[{}, {}, {}, {}],[{}, {}, {}, {}]],  # Helps make the subplot margins small according to https://stackoverflow.com/questions/31526045/remove-space-between-subplots-in-plotly
                                horizontal_spacing=0.1,
                                vertical_spacing=0.1
                            )

                            found_data = False
                            #trace_number = 1

                            for month_index in range(12):

                                # +1 because month_index is zero-based
                                month_number = month_index + 1

                                # subset down to the specific month
                                output_location_protocol_load_month_subset = \
                                    output_location_protocol_load_subset[(output_location_protocol_load_subset.monthnumber == month_number)]

                                # Group by node
                                # Depending on the output type, the node value we are interested in is either the
                                # node reporting the value, or the indexedOutputName which indicates the node the value
                                # is referring to (e.g. for 'Application Received From', the reporting node is the
                                # sink (node 0) but we are interested in the node it is reporting about (indexedOutputName)
                                if 'scatterindex' in output_plot_types:
                                    grp_by_node = output_location_protocol_load_month_subset.groupby(['indexedOutputName'])
                                else:
                                    grp_by_node = output_location_protocol_load_month_subset.groupby(['node'])

                                trace_data_x = []
                                trace_data_y = []
                                trace_data_colour = []

                                for node, group in grp_by_node:

                                    mean_value = group['value'].mean()

                                    #sys.exit()
                                    # Normalise the colour value over the min/max of all values so we get a real 0-1
                                    trace_data_colour.append((mean_value - min_value) / (max_value / min_value))
                                    trace_data_x.append(map_node_to_scatter_location[int(node)][0])
                                    trace_data_y.append(map_node_to_scatter_location[int(node)][1])

                                    found_data = True

                                trace = go.Scatter(
                                    x=trace_data_x,
                                    y=trace_data_y,
                                    mode='markers',
                                    marker=dict(size=14,
                                                line=dict(width=1),
                                                color=trace_data_colour,
                                                #opacity= 0.3
                                                )
                                    #name=protocol_key,
                                    #legendgroup='a'
                                )

                                fig.append_trace(trace,
                                                 row=math.ceil(month_number/4),  # 4 because 4 columns
                                                 col=(month_index % 4) + 1)

                                #fig['layout']['height{}'.format(trace_number)].update(300)
                                #fig['layout']['width{}'.format(trace_number)].update(300)

                                #trace_number = trace_number + 1

                            # Don't attempt to plot if no data, will throw exception
                            if found_data:
                                fig['layout'].update(#height=600, width=600,
                                                     title="{} {} {} {} {}".format(
                                                         simple_output_name,
                                                         indexed_output_name,
                                                         location,
                                                         protocol,
                                                         load_distribution
                                                     ))

                                if not os.path.exists("GenericNodeLayout"):
                                    os.mkdir("GenericNodeLayout")

                                plotly.offline.plot(fig,
                                                    filename=os.path.join("GenericNodeLayout",
                                                                          "{}-{}-{}-{}-{}.html".format(
                                                                              simple_output_name,
                                                                              indexed_output_name,
                                                                              location,
                                                                              protocol,
                                                                              load_distribution
                                                                           )),
                                                    auto_open=False)

                            else:
                                print("No data for {} {}".format(location, protocol))


    else:
        print("Don't know what to do with folder {}".format(results_folder_name))
