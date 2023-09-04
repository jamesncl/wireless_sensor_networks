#!/usr/bin/env python3

import glob
import sys
import re
from collections import defaultdict


def parse_castalia(infolder, filename, measures_to_load):
    """
        Read and parse all Castalia results files found in infolder, output hierarchical dictionary structure.
        Format of output dictionaries:

        results =
        {
              (label, repeat): {
                                  module: {
                                             output_name: {
                                                             value_name: value(int)
                                                              ...
                                                          }
                                          }
                               }
        }

        histograms =
        {
              (label, repeat): {
                                  module: {
                                             (hist_name, hist_min, hist_max): [value1, value2...]
                                          }
                               }
        }

        Note: If there is only 1 value for an output_name, value_name is set to empty string

    :param measures_to_load: list of tuples containin which result output names to load from file. Tuples in form:
                             (<simple_output_name>, <indexed_output_name>, <bool: include_sink_in_statistics?>)
    :param filename: filename to look for, e.g. 'results.txt'
    :param infolder: a directory containing results files. May contain subdirectories. All subdirs will be searched.
    :return: Dictionary as specified above
    """

    is_indexed_value = False

    # recursive=True means, in Python 3.5+, ** is parsed as 'any subfolder'
    results_files = glob.glob('{}**/{}'.format(infolder, filename), recursive=True)

    if not results_files or len(results_files) == 0:
        sys.exit("No results files named {} found in {}".format(filename, infolder))

    # Glob returns in arbitrary order. sort so its consistent
    results_files.sort()

    for results_file in results_files:

        results = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(int))))
        histograms = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))

        print("Processing {}".format(results_file))

        with open(results_file, 'r') as results_reader:

            for line in results_reader:

                if not line.startswith('Castalia|'):
                    sys.exit("Line did not start with Castalia|: {}".format(line))

                # Remove initial Castalia|
                # DONT REMOVE WHITESPACE - tabs can be used to help parse
                line = line.split('|', 1)[1]

                # what:[Lerwick1Min,Lerwick10Min]
                if line.startswith(' what:'):
                    experiment_configuration = line.split(':', 1)[1].strip()

                # when:2017-12-31 14:59
                elif line.startswith(' when:'):
                    experiment_timestamp = line.split(':', 1)[1].strip()

                # repeat:3 label:Lerwick180Min
                elif line.startswith(' repeat:'):

                    re_repeat = re.search(r'repeat:\S+', line)
                    current_repeat = re_repeat.group(0).split(':', 1)[1].strip()
                    # Replace any quotes
                    current_repeat = current_repeat.replace('"', '')
                    current_repeat = int(current_repeat)
                    # print("Repeat is {}".format(current_repeat))

                    re_label = re.search(r'label:\S+', line)
                    current_label = re_label.group(0).split(':', 1)[1].strip()
                    # Remove quotes
                    current_label = current_label.replace('"', '')

                    # print("Label is {}".format(current_label))

                elif line.startswith('\tmodule:'):

                    re_module = re.search(r'module:\S+', line)
                    current_module = re_module.group(0).split(':', 1)[1].strip()
                    # print("Module is {}".format(current_module))

                elif line.startswith('\t\tsimple output name:'):
                    re_output_name = re.search(r'name:.+', line)
                    current_output_name = re_output_name.group(0).split(':', 1)[1].strip()
                    # print("Output name is {}".format(current_output_name))

                elif line.startswith('\t\tsimple output name:'):
                    re_output_name = re.search(r'name:.+', line)
                    current_output_name = re_output_name.group(0).split(':', 1)[1].strip()
                    # print("Output name is {}".format(current_output_name))

                elif line.startswith('\t\t index:'):
                    re_output_name = re.search(r'name:.+', line)
                    current_output_name = re_output_name.group(0).split(':', 1)[1].strip()
                    #print("Indexed output name is {}".format(current_output_name))

                    #  This starts to get tricky. Indexed values are in the form:
                    #  Castalia|                index:1 simple output name:Application Received From
                    #  Castalia|                       9
                    #  Castalia|                index:2 simple output name:Application Received From
                    #  Castalia|                       9
                    # Handle this by setting a flag is_indexed_value = true
                    # This way we can set the value_label without it being overwritten in the next line scan
                    # which would otherwise read it as just a number without a label
                    is_indexed_value = True
                    value_label = line.strip().split(' ', 1)[0].split(':')[1]
                    #print("Index is {}".format(value_label))


                elif line.startswith('\t\t\t'):

                    # Values might be just a number:
                    # \t\t\t139.5
                    # Or a number and a label:
                    # \t\t\t42 Node switched off
                    # So, split on space (at most once)
                    value_tokens = line.strip().split(' ', 1)

                    # float() parsing will handle values in exponent format such as 4.78319e+06
                    value = float(value_tokens[0])

                    if is_indexed_value:
                        # Dont attempt to parse or overwrite value_label - we have already set it
                        pass
                    elif len(value_tokens) == 2:
                        value_label = value_tokens[1]
                    else:
                        # If there is no label, use empty string.
                        # PS If there is no label, this means there will only be one value
                        value_label = ''

                    # Only store result if it is in the list of measures to load
                    # OR if its an indexed value, we need to check only the current_output_name (and not the value_label, which is dynamic)
                    if (current_output_name, value_label) in [(m[0], m[1]) for m in measures_to_load]\
                            or (is_indexed_value and current_output_name in [m[0] for m in measures_to_load]):
                        results[(current_label, current_repeat)][current_module][current_output_name][value_label] = value

                    is_indexed_value = False

                # histogram name:LinkEstimator LQ
                # histogram_min:1 histogram_max:10
                # histogram_values 2430 4 0 0 1 0 0 0 0 0 0
                elif line.startswith('\t\thistogram name:'):

                    re_hist_name = re.search(r'name:.+', line)
                    current_hist_name = re_hist_name.group(0).split(':', 1)[1].strip()
                    # print("Histogram name is {}".format(current_hist_name))

                elif line.startswith('\t\thistogram_min:'):

                    re_hist_min = re.search(r'histogram_min:[0-9]+', line)
                    re_hist_max = re.search(r'histogram_max:[0-9]+', line)
                    current_hist_min = re_hist_min.group(0).split(':', 1)[1].strip()
                    current_hist_max = re_hist_max.group(0).split(':', 1)[1].strip()
                    # print("Histogram min is {}".format(current_hist_min))
                    # print("Histogram max is {}".format(current_hist_max))

                elif line.startswith('\t\thistogram_values'):

                    hist_values = [int(v) for v in line.split(' ')[1:]]
                    # print("Histogram values are {}".format(hist_values))

                    histograms[(current_label, current_repeat)][current_module][(current_hist_name, current_hist_min, current_hist_max)] = hist_values

                else:
                    sys.exit("Unrecognised line {}".format(line))

        yield results, histograms
