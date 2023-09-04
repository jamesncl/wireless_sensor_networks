#!/usr/bin/env python

import os.path
import argparse
import sys
from prettytable import PrettyTable
from prettytable import PLAIN_COLUMNS


def parse_args():
    parser = argparse.ArgumentParser(description="Parse Castalia-Trace.txt file and print tabular")
    parser.add_argument('-f', '--folder', type=str, required=False, default='./',
                        help="Path to folder containing trace file - defaults to current directory")
    parser.add_argument('-i', '--input', type=str, required=False, default='Castalia-Trace.txt',
                        help="Trace filename - defaults to Castalia-Trace.txt")
    parser.add_argument('-n', '--nodeids', type=int, required=True, nargs='*', dest='node_ids',
                        help='Nodes to include in table')
    parser.add_argument('-w', '--width', type=int, required=False, default=120,
                        help='Output table width (not including time column). 120 is good for a widescreen monitor.')
    parser.add_argument('-s', '--start', type=float, required=False,
                        help='Optional time at which to start plotting')
    parser.add_argument('-e', '--end', type=float, required=False,
                        help='Optional time at which to end plotting')
    parser.add_argument('--include', type=str, required=False, nargs='*', dest='include_only_these_msgs',
                        help='Only include messages containing the given strings ')
    parser.add_argument('--modnamedepth', type=int, required=False, default=1,
                        help='How much of the module name label to display in messages, '
                             'in levels - e.g. 1 might be Controller, whereas '
                             '2 might be Mac.Controller')
    parser.add_argument('-m', '--module', type=str, required=False, nargs='*', dest='include_only_these_modules',
                        help='Optional module name(s) to filter by')
    return parser.parse_args()


# This function from https://stackoverflow.com/questions/22560768/add-multiple-line-in-python-single-table-cell
def format_message(comment, max_line_length):

    # accumulated line length
    ACC_length = 0
    words = comment.split(" ")
    formatted_comment = ""
    for word in words:
        # if ACC_length + len(word) and a space is <= max_line_length
        if ACC_length + (len(word) + 1) <= max_line_length:
            # append the word and a space
            formatted_comment = formatted_comment + word + " "
            # length = length + length of word + length of space
            ACC_length = ACC_length + len(word) + 1
        else:
            # append a line break, then the word and a space
            formatted_comment = formatted_comment + "\n" + word + " "
            # reset counter of length to the length of a word and a space
            ACC_length = len(word) + 1
    return formatted_comment


if __name__ == '__main__':
    args = parse_args()

    castalia_file_path = os.path.join(args.folder, args.input)

    if not os.path.isfile(castalia_file_path):
        print "Input file {} does not exist".format(castalia_file_path)
        sys.exit()

    table = PrettyTable(['Time'])
    # fill the table with the specified nodes as empty columns
    for node_id in args.node_ids:
        table.add_column(str(node_id), [])

    f = open(castalia_file_path, 'r')

    for line in f.readlines():
        tokens = line.split()

        # Make sure we have at least 3 tokens (will exclude empty lines, e.g. the first line)
        if len(tokens) >= 3:

            time = float(tokens[0])
            source = tokens[1]
            # Extract the node id from the source - it's between square brackets
            node_id = int(source[source.find('[') + 1:source.find(']')])

            # If the node ID is in the list specified by user
            # and the time is within any optionally specified time frame
            if node_id in args.node_ids \
                and (args.start is None or time >= args.start) \
                and (args.end is None or time <= args.end):

                # Extract the module name from the source
                # The module name is a long chain of component / submodule hierarchy
                # Only take the last args.modnamedepth submodule names to save screen space
                module_name_split = source.rsplit('.')
                module_name = '.'.join(module_name_split[-min(args.modnamedepth, len(module_name_split)):])

                # Filter to show only module name, if specified
                # and messages in inlcude filter, if specified
                if args.include_only_these_modules is None or \
                        any(module_to_include.lower() in module_name.lower() for module_to_include in args.include_only_these_modules):
                    # Message text is the rest of the tokens
                    message = "{}: {}".format(module_name, ' '.join(tokens[2:]))

                    # Filter to show only messages containing any of the substrings specified in include arg
                    if args.include_only_these_msgs is None or \
                            any(string_to_include.lower() in message.lower() for string_to_include in args.include_only_these_msgs):

                        # Add line breaks to create multiline messages to fit into table width
                        # Take the total specified width and divide by the number of node columns to get column width
                        message = format_message(message, args.width / len(args.node_ids))

                        # Make a list with the number of specified nodes as empty string elements
                        node_columns = [''] * len(args.node_ids)
                        # Insert this message into the appropriate column
                        node_columns[args.node_ids.index(node_id)] = message
                        table.add_row([time] + node_columns)

    # table.set_style(PLAIN_COLUMNS)
    table.align = 'l'
    print table #  .get_string(start=0, end=40)