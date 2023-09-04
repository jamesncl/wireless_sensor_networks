#!/usr/bin/env python

import plotly
import plotly.graph_objs as graph_objs
from plotly import tools
import os.path
import argparse
import sys
from collections import defaultdict
from ReadCastaliaTrace import castalia_trace_read


def parse_args():
    parser = argparse.ArgumentParser(description="Plot graphs from Plot-Trace.txt file")
    parser.add_argument('-f', '--folder', type=str, required=False, default='./',
                        help="Path to folder containing Plot-Trace.txt file. Defaults to current directory")
    parser.add_argument('-i', '--input', type=str, required=False, default='Plot-Trace.txt',
                        help="Filename of plot trace input file. Defaults to Plot-Trace.txt")
    parser.add_argument('-o', '--outfolder', type=str, required=False, default='./',
                        help="Output folder. Defaults to current directory")
    parser.add_argument('-n', '--nodeids', type=int, required=False, nargs='*', dest='node_ids',
                        help='Nodes to include in plot. Default is all nodes.')
    parser.add_argument('-a', '--aspects', type=str, required=True, nargs='*', dest='aspects_to_plot',
                        help='Aspect(s) to plot. Choose from any or a combination of '
                             'energy, mac, radio, storage, onoffevent, totaloff, route. '
                             'For example -p energy mac will plot both energy trace and mac events')
    parser.add_argument('-s', '--start', type=float, required=False,
                        help='Optional time at which to start plotting')
    parser.add_argument('-e', '--end', type=float, required=False,
                        help='Optional time at which to end plotting')
    parser.add_argument('--include', type=str, required=False, nargs='*', dest='include_only_these_codes',
                        help='Only include codes containing the given strings (space separated) as substrings, '
                             'case insensitive')
    parser.add_argument('--exclude', type=str, required=False, nargs='*', dest='exclude_these_codes',
                        help='Exclude codes containing the given strings (space separated) as substrings, '
                             'case insensitive')
    parser.add_argument('-r', '--run', type=int, required=False,
                        help='For simulations with multiple repetitions, this is to specify which run (repetition)'
                             'to plot. Otherwise, data from all repetitions will be overlayed.')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()

    castalia_file_path = os.path.join(args.folder, args.input)

    if not os.path.isfile(castalia_file_path):
        print "Input file {} does not exist".format(castalia_file_path)
        sys.exit()

    # First just read the 3 experiment labels contained in the first 3 lines of the file
    #
    # experiment-label:Loc-Ler-Gra-1-Sta-59-0800-Days-1-CtpTmac-singleNode-Short
    # measurement-label:startSeconds--5126400
    # repetition-label:0

    with open(castalia_file_path, 'r') as trace_file:
        # Split on :, strip whitespace, remove all '-'
        experiment_labels = [next(trace_file).split(":", 1)[1].strip().replace('-', '') for x in xrange(3)]

    experiment_labels_filename = '{}-{}-rep{}'.format(experiment_labels[0], experiment_labels[1], experiment_labels[2])

    subplots_out_path = os.path.join(args.outfolder, "pt_{}_{}.html".format(experiment_labels_filename,
                                                                            '_'.join(args.aspects_to_plot)))
    totaloff_out_path = os.path.join(args.outfolder, "pt_{}_total_off.html".format(experiment_labels_filename))

    # Example Castalia-Trace.txt format is
    # 9.847809919267    SN.node[2].Communication.MAC.Sender     Received 'okay to send'
    # 9.847809919267    SN.node[2].Communication.MAC.Sender     #SEND Starting the next message train in the queue
    #
    # For scatter plot we want to extract:
    # time - this is x axis
    # node id - this is y axis
    # #CODE - this is the colour / symbol

    # Using defaultdict which automatcally initialises an entry if a key doesn't exist

    # A dictionary for individual node's event scatter charts - dictionary entries represent a node, which contains a
    # sub-dictionary of event codes, which contains a dictionary of graph data lists
    events_by_node_and_event_code = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))

    # A dictionary for energy line charts - dictionary entries represent a node, which contains a
    # sub-dictionary of graph data lists
    energy_consumption_data_by_node = defaultdict(lambda: defaultdict(list))

    # A dictionary for storage line charts - dictionary entries represent a node, which contains a
    # sub-dictionary of graph data lists
    storage_energy_data_by_node = defaultdict(lambda: defaultdict(list))

    # A dictionary of graph data lists. This is for holding a simple running total of the number of nodes
    # switched off over time. i.e. 'x' will be time, 'y' will be the number of nodes switched off
    running_total_off_trace = defaultdict(list)
    # We will also need a running total variable
    # Ideally we want to initialise the running total of 'number of nodes off' as the number of nodes
    # in the simulation (e.g. 17), because they all start in the off state. However we don't know
    # beforehand how many nodes were run in the simulation. We only know this after running through
    # all the data. Therefore start the running total at zero, and we deduct the number of nodes
    # from the running total later, when graphing
    running_total_off = 0

    # A list of all nodes which we discover data for (this is used later when constructing graphs)
    # We could use the -n argument, but as this is optional we may not have any (defaults to all nodes)
    # Therefore safest to make a list of nodes we find as we go through the data
    nodes_discovered = []

    # A counter to keep track of which run / repetition we are currently on. For simulations with multiple
    # repetitions, data from successive repetitions are appended to the end of the previous.
    # We check the args.run argument and plot only the run specified.
    current_run = 0

    # To create a *stepped* line graph for energy, for every energy value data point, we will add another data
    # point representing the previous energy value at the same timestamp. Use this to hold the previous value:
    # dictionary of previous energy values for each node_id defaulting to zero
    # Update: ALSO used for updating chart with relative energy values
    # To do this, we need to store a separate value for each module, hence the nested dictionary for node, then module
    previous_energy_values = defaultdict(lambda: defaultdict(lambda: 0))
    current_energy_values = defaultdict(lambda: defaultdict(lambda: 0))

    for trace_item in castalia_trace_read(castalia_file_path, args.include_only_these_codes, args.exclude_these_codes):

        # Only process nodes which we have specified (if none specified, process all nodes)
        # and only process within any optionally specified time frame
        # and only process events in any optional include
        if (args.node_ids is None or trace_item.node_id in args.node_ids)\
                and (args.start is None or trace_item.time >= args.start)\
                and (args.end is None or trace_item.time <= args.end):

            # Add to the list of nodes we've discovered (note - not using a dictionary because it would be useful
            # to refer to the index of the node when looping through - so prefer to use a list here)
            if trace_item.node_id not in nodes_discovered:
                nodes_discovered.append(trace_item.node_id)

            # If we're generating plots which require event data,
            # and the first message string contains an event-related code
            # Note: for on-off events, ignoring time=0
            if ("mac" in args.aspects_to_plot and "#MAC" in trace_item.code) or \
                    ("radio" in args.aspects_to_plot and "#RAD" in trace_item.code) or \
                    ("onoffevent" in args.aspects_to_plot and "#ONOFF" in trace_item.code and trace_item.time != 0) or \
                    ("route" in args.aspects_to_plot and "#ROU" in trace_item.code):
                # This is an event. Add to the event data
                # Also add the event data to a seperate dictionary with an entry for each node
                # Using 20 as the baseline y value was just an arbitrary decision so that on the overlay graphs of
                # events and energy, the events will appear roughly in the middle of the energy traces
                events_by_node_and_event_code[trace_item.node_id][trace_item.code]['x'].append(trace_item.time)
                events_by_node_and_event_code[trace_item.node_id][trace_item.code]['y'].append(20 + trace_item.y_jitter)
                events_by_node_and_event_code[trace_item.node_id][trace_item.code]['colour'].append(trace_item.colour)
                events_by_node_and_event_code[trace_item.node_id][trace_item.code]['symbol'].append(trace_item.symbol)
                events_by_node_and_event_code[trace_item.node_id][trace_item.code]['extra'].append(trace_item.extra)

            # If we're generating plots which require energy consumption trace data,
            # and the first message string contains ENG
            elif "energy" in args.aspects_to_plot and "#ENG" in trace_item.code:
                # This is an energy event. Add to the line data

                # First we need to determine if this is a RELATIVE energy value (not ABSOLUTE)
                # If it is, we need to calculate the new energy as the last energy plus / minus the new one
                energy = trace_item.energy

                if "#ENG_REL" in trace_item.code:
                    energy = previous_energy_values[trace_item.node_id][trace_item.module_name] + energy

                # Store the new energy value
                current_energy_values[trace_item.node_id][trace_item.module_name] = energy

                # To create a *stepped* line graph we add a data point for the previous energy
                # (note we need the TOTAL energy from all the modules)
                energy_consumption_data_by_node[trace_item.node_id]['x'].append(trace_item.time)
                energy_consumption_data_by_node[trace_item.node_id]['y'].append(sum(previous_energy_values[trace_item.node_id].itervalues()))

                # Add the data point (note we need the TOTAL energy from all the modules)
                energy_consumption_data_by_node[trace_item.node_id]['x'].append(trace_item.time)
                energy_consumption_data_by_node[trace_item.node_id]['y'].append(sum(current_energy_values[trace_item.node_id].itervalues()))

                # Update the previous energy dictionary
                previous_energy_values[trace_item.node_id][trace_item.module_name] = energy

            # If we're generating plots which require storage energy data,
            # and the first message string contains STO
            # STO means storage, referring to energy in storage devices (supercapacitors etc)
            elif "storage" in args.aspects_to_plot and "#STO" in trace_item.code:

                storage_energy_data_by_node[trace_item.node_id]['x'].append(trace_item.time)
                storage_energy_data_by_node[trace_item.node_id]['y'].append(trace_item.energy)

            # If we're generating plots which require a running total trace of nodes turned off,
            # and the first message string contains ONOFF
            # Ignoring Time = 0 as Castalia outputs lots of spurious on/off events when initialising
            elif "totaloff" in args.aspects_to_plot and "#ONOFF_" in trace_item.code and trace_item.time != 0:

                # Ideally we want to initialise the running total of 'number of nodes off' as the number of nodes
                # in the simulation (e.g. 17), becuase they all start in the off state. However we don't know
                # beforehand how many nodes were run in the simulation. We only know this after running through
                # all the data. Therefore we deduct the number of nodes from the running total later, when graphing
                if trace_item.code == "#ONOFF_OFF":
                    running_total_off += 1
                elif trace_item.code == "#ONOFF_ON":
                    running_total_off -= 1

                # Add ON-OFF events to a dictionary with an entry for each node
                # Using 30 as the baseline y value was just an arbitrary decision so that on the overlay graphs of
                # ON/OFF events and storage energy, the events will appear roughly in the middle of the energy trace
                running_total_off_trace['x'].append(trace_item.time)
                running_total_off_trace['y'].append(running_total_off)

    #######################
    # Plotting
    #######################

    # Sort the list of discovered nodes, so subplots show nodes in numerical order
    nodes_discovered.sort()

    #######################
    # Page of subplots, 1 subplot per node, with specified aspects overlayed

    # Figure out what the labels for the subplots are - label with node id according to nodes discovered
    subplot_titles = []
    for node in nodes_discovered:
        subplot_titles.append("Node {}".format(node))

    # Initialise subplot structure - each node on a separate row, all in 1 column
    fig = tools.make_subplots(rows=len(nodes_discovered), cols=1, subplot_titles=subplot_titles,
                              shared_xaxes=True, shared_yaxes=True)

    # For every node we have data for
    for node in nodes_discovered:

        row_index = nodes_discovered.index(node) + 1

        if "mac" in args.aspects_to_plot or \
                "radio" in args.aspects_to_plot or \
                "onoffevent" in args.aspects_to_plot or \
                "route" in args.aspects_to_plot:

            for code, code_trace_data in events_by_node_and_event_code[node].iteritems():

                # Remove #MAC #ROU #RAD to shorten name, otherwise sometimes doesnt fit
                code = code.replace('#ROU_', '')
                code = code.replace('#MAC_', '')
                code = code.replace('#RAD_', '')

                fig.append_trace(
                    graph_objs.Scatter(
                        x=code_trace_data['x'],
                        y=code_trace_data['y'],
                        mode='markers+text',
                        name=code,
                        text=code_trace_data['extra'],
                        hoverinfo='name+text',
                        marker=dict(
                            size='16',
                            color=code_trace_data['colour'],
                            symbol=code_trace_data['symbol']
                        )
                    ),

                    row=row_index,  # Row
                    col=1  # Column
                )

        if "energy" in args.aspects_to_plot:
            fig.append_trace(
                graph_objs.Scatter(
                    x=energy_consumption_data_by_node[node]['x'],
                    y=energy_consumption_data_by_node[node]['y'],
                    name=node),

                row=row_index,  # Row
                col=1  # Column
            )

        if "storage" in args.aspects_to_plot:
            fig.append_trace(
                graph_objs.Scatter(
                    x=storage_energy_data_by_node[node]['x'],
                    y=storage_energy_data_by_node[node]['y'],
                    name=node),

                row=row_index,  # Row
                col=1  # Column
            )

    try:
        plotly.offline.plot(fig, filename=subplots_out_path, auto_open=False)
    except plotly.exceptions.PlotlyEmptyDataError:
        pass
        # Do nothing - it's okay if there's no data to plot. There may be other plots

    ########################
    # Other plots

    # Running total of number of nodes turned off over time
    # Doing this as a separate plot because this graph, unlike all the others, doesn't have data points per node
    # It has data points representing no of all nodes turned off

    if "totaloff" in args.aspects_to_plot:

        # We have to add the number of nodes (len(nodes_discovered)) from the running total of off nodes
        # to get a sensible non-negative number. The running total was initialised at zero - it should have been
        # initialised as the number of nodes, but we don't know beforehand how many nodes were simulated
        total_off_trace = graph_objs.Scatter(
            x=running_total_off_trace['x'],
            y=[y + len(nodes_discovered) for y in running_total_off_trace['y']]
        )

        total_off_data = graph_objs.Data([total_off_trace])

        plotly.offline.plot(total_off_data, filename=totaloff_out_path, auto_open=False)
