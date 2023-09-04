#!/usr/bin/env python

import os
import random
from namedlist import namedlist


def castalia_trace_read(castalia_file_path, include_only_these_codes, exclude_these_codes):
    """
        Reads the Plot-Trace.txt file, parses and returns the data as a generator

        :param castalia_file_path: The trace output file from Castalia (Plot-Trace.txt)
        :return: a namedtuple containing the parsed data
    """

    if not os.path.isfile(castalia_file_path):
        raise ValueError('File {} not found'.format(castalia_file_path))

    # Define some tuple types we are going to return based on what each line of data contains
    GenericTraceData = namedlist('GenericTraceData',
                                  'time source code node_id y_jitter colour symbol extra')
    RoutingNewParentTraceData = namedlist('RoutingNewParentTraceData',
                                           'time source code node_id y_jitter colour symbol new_parent_node_id extra')
    RoutingUpdateMhEtxTraceData = namedlist('RoutingUpdateMhEtxTraceData',
                                             'time source code node_id y_jitter colour symbol parent_node_id mhetx extra')
    MacScheduleNewPrimary = namedlist('MacScheduleNewPrimary',
                                      'time source code node_id y_jitter colour symbol schedule')
    MacScheduleNewSecondary = namedlist('MacScheduleNewSecondary',
                                        'time source code node_id y_jitter colour symbol schedule')
    EnergyConsumptionTraceData = namedlist('EnergyConsumptionTraceData',
                                            'time source code node_id module_name energy')
    StorageEnergyTraceData = namedlist('StorageEnergyTraceData',
                                        'time source code node_id voltage energy')

    # For each unique code we read which are to be represented with graph markers (i.e. not energy line traces),
    # we're going to generate a random y jitter
    # (used for plotting markers which don't overlap),
    # a random colour and random shape for the marker
    # Use this dicitonary to hold them:
    code_marker_attributes = {}
    # Set the seed for random so we always get the same random numbers (colours for markers) on repeated runs
    random.seed(1)

    with open(castalia_file_path, 'r') as trace_file:
        for line in trace_file.readlines():
            tokens = line.split()

            # Ignore lines without at least 3 strings.
            # This will skip over any blank lines, and also the experiment labels at the head of the file, e.g:
            #
            # experiment-label:Loc-Ler-Gra-1-Sta-59-0800-Days-1-CtpTmac-singleNode-Short
            # measurement-label:startSeconds--5126400
            # repetition-label:0

            if len(tokens) >= 3:

                # The first 3 tokens are always time, source and a code
                time = float(tokens[0])

                # Ignore events at time zero
                if time > 0:
                    source = tokens[1]
                    code = tokens[2]
                    # Extract the node id from the source - it's between square brackets
                    node_id = int(source[source.find('[') + 1:source.find(']')])

                    # Filter in / out codes as specified by args
                    if (include_only_these_codes is None or any(include_code.lower() in code.lower() for include_code in include_only_these_codes))\
                            and (exclude_these_codes is None or not any(exclude_code.lower() in code.lower() for exclude_code in exclude_these_codes)):

                        # Depending on the code, we return different types of namedtuple
                        # First we handle energy trace data, which are plotted as lines and so do not have marker attributes
                        if "#ENG_" in code:

                            # Energy trace messages are in the format '#CODE ModuleName EnergyValue'
                            # e.g. '#ENG_REL Radio 55.7'
                            module_name = tokens[3]
                            energy = float(tokens[4])

                            yield EnergyConsumptionTraceData(
                                time, source, code, node_id, module_name, energy)

                        elif "#STO" in code:
                            voltage = float(tokens[3])
                            energy = float(tokens[4])

                            yield StorageEnergyTraceData(
                                time, source, code, node_id, voltage, energy)

                        # All other codes are events, so we also need to generate marker attributes for them
                        else:

                            # If we haven't already generated a set of marker attributes for this code
                            if code not in code_marker_attributes:

                                # Generate them
                                y_jitter = float(len(code_marker_attributes) + 1) / 0.5
                                colour = 'rgb({}, {}, {})'.format(
                                    random.randint(20, 255),
                                    random.randint(20, 255),
                                    random.randint(20, 255))

                                symbol = len(code_marker_attributes) + 1

                                # Symbols 33 and 34 seem to be invisible! Do not use
                                if symbol == 33:
                                    symbol = 1
                                elif symbol == 34:
                                    symbol = 2

                                # print "Code {} is colour {} symbol {}".format(code, colour, symbol)

                                # And add them to the list
                                code_marker_attributes[code] = (y_jitter, colour, symbol)

                            if "#ROU_PARENT" in code:
                                new_parent_node_id = int(tokens[3])

                                # If there is an extra token (tokens[3]) include it as extra
                                if len(tokens) >= 4:
                                    extra = tokens[3]
                                else:
                                    extra = None

                                yield RoutingNewParentTraceData(
                                    time, source, code, node_id,
                                    code_marker_attributes[code][0],  # y_jitter
                                    code_marker_attributes[code][1],  # colour
                                    code_marker_attributes[code][2],
                                    new_parent_node_id,
                                    extra)  # extra

                            elif "#ROU_MHETX" in code:
                                parent_node_id = int(tokens[3])
                                mhetx = float(tokens[4])

                                yield RoutingUpdateMhEtxTraceData(
                                    time, source, code, node_id,
                                    code_marker_attributes[code][0],  # y_jitter
                                    code_marker_attributes[code][1],  # colour
                                    code_marker_attributes[code][2],
                                    parent_node_id, mhetx,
                                    None)  # extra

                            elif "#MAC_SCHED_PRIMARY" in code:
                                schedule = int(tokens[3])

                                yield MacScheduleNewPrimary(
                                    time, source, code, node_id,
                                    code_marker_attributes[code][0],  # y_jitter
                                    code_marker_attributes[code][1],  # colour
                                    code_marker_attributes[code][2],
                                    schedule)

                            elif "#MAC_SCHED_SECONDARY" in code:
                                schedule = int(tokens[3])

                                yield MacScheduleNewSecondary(
                                    time, source, code, node_id,
                                    code_marker_attributes[code][0],  # y_jitter
                                    code_marker_attributes[code][1],  # colour
                                    code_marker_attributes[code][2],
                                    schedule)

                            else:

                                # If there is an extra token (tokens[3]) include it as extra
                                if len(tokens) >= 4:
                                    extra = tokens[3]
                                else:
                                    extra = None

                                yield GenericTraceData(time, source, code, node_id,
                                                       code_marker_attributes[code][0],  # y_jitter
                                                       code_marker_attributes[code][1],  # colour
                                                       code_marker_attributes[code][2],  # symbol
                                                       extra)
