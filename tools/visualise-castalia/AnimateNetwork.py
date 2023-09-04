#!/usr/bin/env python

import sys
import os
import argparse
import warnings
import select
import shutil
import matplotlib
from matplotlib.pyplot import pause
import networkx as nx
from ReadCastaliaTrace import castalia_trace_read
from CombineFramesToVideo import make_video


def parse_args():
    parser = argparse.ArgumentParser(description="Animate network graph using Plot-Trace.txt trace file data")
    parser.add_argument('-f', '--folder', type=str, required=False, default='./',
                        help="Path to folder containing Plot-Trace.txt file. Defaults to current directory")
    parser.add_argument('-i', '--input', type=str, required=False, default='Plot-Trace.txt',
                        help="Filename of the trace file. Defaults to Plot-Trace.txt")
    parser.add_argument('-o', '--outfolder', type=str, required=False, default='./',
                        help="Output folder. Defaults to current directory")
    parser.add_argument('-d', '--dimensions', type=str, required=True,
                        help="The grid dimensions of the nodes simulated, EXCLUDING THE SINK. e.g. 4x4, 2x2, 6x2")
    parser.add_argument('-s', '--sinklocation', type=str, required=True,
                        help="The XxY location of the sink. e.g. 0.5x0.5 would be the centre of a 2x2 grid")
    parser.add_argument('-v', '--video', action='store_true',
                        help="If set, save the animation as a video")
    parser.add_argument('-b', '--begin', type=float, required=False,
                        help='Optional time at which to start animating')
    parser.add_argument('-e', '--end', type=float, required=False,
                        help='Optional time at which to end animating')
    parser.add_argument('--schedules', action='store_true',
                        help="If set, show schedules as node labels instead of node ID")
    parser.add_argument('--hide', action='store_true',
                        help="If set, do not show the frames while processing")
    parser.add_argument('--colourenergy', action='store_true',
                        help="If set, colour nodes according to energy")
    parser.add_argument('--minenergy', type=float, required=False, default=20,
                        help='minimum energy for colour map')
    parser.add_argument('--maxenergy', type=float, required=False, default=30,
                        help='maximum energy for colour map')
    return parser.parse_args()


def remove_all_edges_from_node(graph, node_id):
    """

    :param graph: The graph to operate on
    :param node_id: The node to remove edges from
    :return: True if edges were removed, False if there were no edges to remove
    """

    # Get edges from the node
    # For directed graphs, nx.edges returns just the out-edges
    all_edges_from_node = nx.edges(graph, [node_id])
    if len(all_edges_from_node) > 0:
        graph.remove_edges_from(all_edges_from_node)
        return True

    return False


def draw_graph(graph, time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, save_frame, folder_to_save_to):

    #print(colours)
    nx.draw(graph,
            pos=nx.get_node_attributes(graph, 'pos'),
            node_color=colours,
            vmin=0, vmax=1,
            cmap=cmap,
            with_labels=(False if args.schedules else True),  # If user chooses to show schedule labels, do not show node ID labels
            arrows=True)

    if args.schedules:

        # Form schedule label from primary and secondary. Must be a dict (networkx needs labels in a dict)
        node_labels_schedule_combined = {}

        # Need to search schedule labels for any reported schedules. There may not be any, in which case use empty string.
        for i in range(graph.number_of_nodes()):
            # i is the node ID
            node_labels_schedule_combined[i] = '{}{}'.format(node_labels_schedule_primary.get(i, ''),
                                                             '({})'.format(node_labels_schedule_secondary.get(i))
                                                                if node_labels_schedule_secondary.has_key(i) and node_labels_schedule_secondary[i] is not None else '')

        nx.draw_networkx_labels(graph,
                                pos=nx.get_node_attributes(graph, 'pos'),
                                labels=node_labels_schedule_combined)

    nx.draw_networkx_edge_labels(graph,
                                 pos=nx.get_node_attributes(graph, 'pos'),
                                 edge_labels=edge_labels)

    if save_frame:

        # The PNG file name must contain an integer as the frame number. So take the time
        # (e.g. 0.000937400513) mutiply by 1,000,000,000 and round (937400)
        frame_number = int(round(time * 1000000000))
        matplotlib.pyplot.savefig(os.path.join(folder_to_save_to, '{:015d}.png'.format(frame_number)))

    if not args.hide:
        # pause animation a tiny bit so it displays for the user
        pause(0.0000001)


if __name__ == '__main__':
    args = parse_args()

    if not os.path.isdir(args.folder):
        print "Input folder does not exist"
        sys.exit()

    if not os.path.isdir(args.outfolder):
        print "Output folder does not exist"
        sys.exit()

    castalia_file_path = "{}{}".format(args.folder, args.input)

    if not os.path.isfile(castalia_file_path):
        print "Input file {} does not exist".format(castalia_file_path)
        sys.exit()

    x_y_dimensions = args.dimensions.split('x')
    x_y_sink_location = args.sinklocation.split('x')
    if not len(x_y_dimensions) == 2 or not len(x_y_sink_location) == 2:
        print "Incorrect dimensions and / or sink location argument syntax. Must be NxM"
        sys.exit()

    try:
        x_dimension = int(x_y_dimensions[0])
        y_dimension = int(x_y_dimensions[1])
    except ValueError:
        print "Incorrect dimensions argument syntax (NxM). N and M must be integers"
        sys.exit()

    try:
        x_sink_location = float(x_y_sink_location[0])
        y_sink_location = float(x_y_sink_location[1])
    except ValueError:
        print "Incorrect sink location argument syntax (NxM). N and M must be floats"
        sys.exit()

    # Calc number of nodes
    # This is the grid x dimension * y dimension plus 1 for the sink
    no_of_nodes = x_dimension * y_dimension + 1

    # Calc the plot margins we want
    plot_area_top_padding = 0.8
    plot_area_bottom_left_right_padding = 0.2
    plot_area_x_start = 0 - plot_area_bottom_left_right_padding
    plot_area_x_end = x_dimension - 1 + plot_area_bottom_left_right_padding  # -1 because axis starts at 0 not 1
    plot_area_y_start = 0 - plot_area_bottom_left_right_padding
    plot_area_y_end = y_dimension - 1 + plot_area_top_padding  # -1 because axis starts at 0 not 1

    # and the position of the title - should be at the top, approx in the middle
    plot_title_y_pos = plot_area_y_end - 0.2
    plot_title_x_pos = (plot_area_x_end / 2) - 0.2  # minus a bit so title text is hopefully approx centre

    # Check the given output folder name ends with slash, if not add it
    if not args.outfolder.endswith("/"):
        args.outfolder = "{}/".format(args.outfolder)

    # If we need to create a video
    if args.video:

        # work out the temp folder we will add PNGs frames to
        temp_video_folder = os.path.join(args.outfolder, 'video_tmp')

        # If it already exists, delete it and contents
        if os.path.exists(temp_video_folder):
            shutil.rmtree(temp_video_folder)

        # Create the temp folder
        os.makedirs(temp_video_folder)
    else:
        temp_video_folder = None

    # We (may) also want to save some interesting frames, e.g. frames where a loop occurrs. Create a folder for these.
    interesting_frames_folder = os.path.join(args.outfolder, 'video_interesting_frames')

    # If it already exists, delete it and contents
    if os.path.exists(interesting_frames_folder):
        shutil.rmtree(interesting_frames_folder)

    # Create the temp folder
    os.makedirs(interesting_frames_folder)

    # Initialise the graph as a grid
    graph = nx.DiGraph()

    # Add the sink node (always node 0)
    graph.add_node(0, pos=(x_sink_location, y_sink_location))

    # Define node colours
    # b: blue
    # g: green
    # r: red
    # c: cyan
    # m: magenta
    # y: yellow
    # k: black
    # w: white


    if args.colourenergy:
        colour_on = 1.0
        colour_off = 0.0
    else:
        colour_on = 'g'
        colour_off = 'r'

    colour_pull_initiated = 'b'
    colour_pull_received = 'c'
    colour_loop_detected = 'k'
    colour_send = 'y'
    colour_forward = 'm'
    #colour_flash_duration = 1  # How many frames to use when flashing a colour for before reverting back

    # Create colour map
    cmap = matplotlib.colors.LinearSegmentedColormap.from_list("", ["red", "orangered", "orange", "yellow", "green"])

    # Initialise all nodes as 'on' colour
    colours = [colour_on] * no_of_nodes

    # Define node labels and send indicators
    # node_label_send_indicator_flip = ':'
    # node_label_send_indicator_flop = '.'
    # # Initialise all nodes send indicators
    # node_labels = {}
    # for node in range(0, no_of_nodes):
    #     node_labels[node] = '{}{}'.format(str(node), node_label_send_indicator_flop)

    # Define schedule labels
    node_labels_schedule_primary = {}
    node_labels_schedule_secondary = {}

    # Add the other nodes as a grid
    # E.g. for a 2x2 grid, we want:
    # 1     2
    #
    # 3     4
    #
    # So add in this order:
    # 1: x=0 y=1
    # 2: x=1 y=1
    # 3: x=0 y=0
    # 4: x=1 y=0
    node_count = 1
    for y in range(y_dimension - 1, -1, -1):  # Increment backwards. -1 end because range doesn't include the end.
        for x in range(0, x_dimension):
            graph.add_node(node_count, pos=(x, y))
            node_count += 1

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        warnings.warn("deprecated", DeprecationWarning)
        #matplotlib.pyplot.show()
        nx.draw(graph, pos=nx.get_node_attributes(graph, 'pos'), node_color=colours,
                vmin=0, vmax=1,
                cmap=cmap,
                )
        matplotlib.pyplot.draw()
        last_update_time = 0.0
        edge_labels = {}
        advance_next = False
        number_messages_received_by_sink = 0

        print "Hit return to pause"

        for trace_item in castalia_trace_read(castalia_file_path,
                                              ["#ONOFF_OFF", "#ONOFF_ON", "#ROU_PARENT", "#ROU_MHETX",
                                               "#ROU_SEND_BEACON_WITH_PULL", "#ROU_PULL_RECEIVED", "#ROU_LOOP_DETECTED",
                                               "#ROU_SEND", "#ROU_REC_MSG_TO_FORWARD", "#APP_RECD", "#ROU_INVALID_PARENT",
                                               "#MAC_SCHED_PRIMARY", "#MAC_SCHED_SECONDARY", "#STO"],
                                              None):

            if (args.begin is None or trace_item.time >= args.begin) \
                    and (args.end is None or trace_item.time <= args.end):

                # If this event occurred at the same time as the previous event,
                # we need to fudge a small time difference so that we don't lose
                # a frame (frames with the same time will overwrite each other
                if trace_item.time == last_update_time:
                    trace_item.time += 0.00000001
                last_update_time = trace_item.time

                # Check if the user has hit enter (non-blocking)
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0] or advance_next:
                    advance_next = False

                    # If yes, pause
                    line = raw_input()
                    user_input = raw_input("Paused at {}. Type 'n' then return to advance 1 event. "
                                           "Type 'v' then return to stop here and create video. "
                                           "Press return to continue."
                                           .format(trace_item.time))
                    if user_input.lower() == 'n':
                        advance_next = True
                    elif user_input.lower() == 'v':
                        break
                    print "Resuming"

                # Clear the graph - we are overlaying on same plot so need to wipe the canvas
                matplotlib.pyplot.clf()
                matplotlib.pyplot.axis((plot_area_x_start, plot_area_x_end, plot_area_y_start, plot_area_y_end))

                # APPLICATION RECEIVED MESSAGE
                # If we receive an application received event, increment the counter before the plot title is printed
                if trace_item.code == "#APP_RECD":
                    number_messages_received_by_sink += 1

                # Add the time and receive count as a title
                matplotlib.pyplot.text(plot_title_x_pos, plot_title_y_pos, s="time: {:.12f} rec: {}".format(
                    trace_item.time, number_messages_received_by_sink), horizontalalignment='center')

                # ROUTE AND SCHEDULE CHANGE EVENTS
                # These events change the colour of the node, node labels, the node edges and / or the edge labels
                # We need to make the changes to the underlying graph
                # Then redraw everything once
                if trace_item.code in ("#ONOFF_OFF", "#ONOFF_ON", "#ROU_PARENT", "#ROU_MHETX", "#ROU_INVALID_PARENT",
                                       "#MAC_SCHED_PRIMARY", "#MAC_SCHED_SECONDARY", "#STO"):
                    if trace_item.code == "#ONOFF_OFF":
                        colours[trace_item.node_id] = colour_off
                        # Also remove all edges from a node which turns off
                        remove_all_edges_from_node(graph, trace_item.node_id)
                        # Also remove all schedules
                        node_labels_schedule_primary[trace_item.node_id] = None
                        node_labels_schedule_secondary[trace_item.node_id] = None

                    elif trace_item.code == "#ONOFF_ON":
                        colours[trace_item.node_id] = colour_on

                    elif trace_item.code == "#ROU_PARENT":
                        # New parent, so remove any existing edge to old parent
                        remove_all_edges_from_node(graph, trace_item.node_id)
                        graph.add_edge(trace_item.node_id, trace_item.new_parent_node_id)
                        #colours[trace_item.node_id] = colour_on

                    elif trace_item.code == "#ROU_INVALID_PARENT":
                        # Parent has been removed (e.g. unreachable), remove all edges from node
                        remove_all_edges_from_node(graph, trace_item.node_id)

                    elif trace_item.code == "#ROU_MHETX":
                        # New MH ETX, so remove any existing edge with old MHETX label
                        remove_all_edges_from_node(graph, trace_item.node_id)
                        graph.add_edge(trace_item.node_id, trace_item.parent_node_id, mhetx=round(trace_item.mhetx, 2))
                        # Update edge labels
                        edge_labels = nx.get_edge_attributes(graph, 'mhetx')
                        #colours[trace_item.node_id] = colour_on

                    elif trace_item.code == "#MAC_SCHED_PRIMARY":
                        # New primary schedule, so replacing:
                        node_labels_schedule_primary[trace_item.node_id] = trace_item.schedule

                    elif trace_item.code == "#MAC_SCHED_SECONDARY":
                        # New secondary schedule, so ADDING (there can be more than 1):
                        # If there are none yet, create a new list
                        if node_labels_schedule_secondary.get(trace_item.node_id) is None:
                            node_labels_schedule_secondary[trace_item.node_id] = [trace_item.schedule]
                        # Otherwise, append to the existing list of secondary schedules
                        else:
                            node_labels_schedule_secondary[trace_item.node_id].append(trace_item.schedule)

                    elif args.colourenergy and trace_item.code == "#STO" and colours[trace_item.node_id] != colour_off:
                        #pass
                        #print("Setting colour for node " + str(trace_item.node_id) + " to " + str(trace_item.energy))
                        # Normalise to 0-1

                        colours[trace_item.node_id] = (trace_item.energy-args.minenergy)/(args.maxenergy-args.minenergy)

                    draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, args.video, temp_video_folder)

                # COLOUR FLASH EVENTS
                # These events just cause the colour of a node to flash for a period of time
                # We need to redraw the canvas multiple times (done by the flash_colour function)

                else:

                    # Plot loop detection events even if colouring by energy
                    if trace_item.code == "#ROU_LOOP_DETECTED":
                        # Flash send colour for a small period of time
                        original_colour = colours[trace_item.node_id]
                        # flash_colour(graph, trace_item.node_id, colour_loop_detected, colour_flash_duration, colours,
                        #              edge_labels, trace_item.time, args.video, temp_video_folder)
                        colours[trace_item.node_id] = colour_loop_detected
                        draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, args.video, temp_video_folder)
                        # Also, this is an intersting frame - want to check what loops are occurring.
                        # Draw the graph again, this time saving to the intersting frames folder
                        draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, True, interesting_frames_folder)
                        # revert colour back
                        colours[trace_item.node_id] = original_colour

                    # For other events, only show colour flash if not colouring by energy
                    if not args.colourenergy:  # other events must be colour flash events

                        if trace_item.code == "#ROU_SEND":
                            # Flash send colour for a small period of time
                            original_colour = colours[trace_item.node_id]
                            # flash_colour(graph, trace_item.node_id, colour_send, colours, edge_labels,
                            #              trace_item.time, args.video, temp_video_folder)
                            colours[trace_item.node_id] = colour_send
                            draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, args.video, temp_video_folder)
                            # revert colour back
                            colours[trace_item.node_id] = original_colour

                        elif trace_item.code == "#ROU_REC_MSG_TO_FORWARD":
                            # Flash send colour for a small period of time
                            original_colour = colours[trace_item.node_id]
                            # flash_colour(graph, trace_item.node_id, colour_forward, colour_flash_duration, colours, edge_labels,
                            #              trace_item.time, args.video, temp_video_folder)
                            colours[trace_item.node_id] = colour_forward
                            draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, args.video, temp_video_folder)
                            # revert colour back
                            colours[trace_item.node_id] = original_colour

                        elif trace_item.code == "#ROU_SEND_BEACON_WITH_PULL":
                            # Flash send colour for a small period of time
                            original_colour = colours[trace_item.node_id]
                            # flash_colour(graph, trace_item.node_id, colour_pull_initiated, colour_flash_duration, colours,
                            #              edge_labels, trace_item.time, args.video, temp_video_folder)
                            colours[trace_item.node_id] = colour_pull_initiated
                            draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, args.video, temp_video_folder)
                            # revert colour back
                            colours[trace_item.node_id] = original_colour

                        elif trace_item.code == "#ROU_PULL_RECEIVED":
                            # Flash send colour for a small period of time
                            original_colour = colours[trace_item.node_id]
                            # flash_colour(graph, trace_item.node_id, colour_pull_received, colour_flash_duration, colours,
                            #              edge_labels, trace_item.time, args.video, temp_video_folder)
                            colours[trace_item.node_id] = colour_pull_received
                            draw_graph(graph, trace_item.time, colours, edge_labels, node_labels_schedule_primary, node_labels_schedule_secondary, args.video, temp_video_folder)
                            # revert colour back
                            colours[trace_item.node_id] = original_colour

        if args.video:

            make_video(temp_video_folder, args.outfolder)

            # Delete the temp file
            # if os.path.exists(temp_video_folder):
            #     shutil.rmtree(temp_video_folder)
