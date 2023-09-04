#!/usr/bin/env python3

import argparse
import os
import sys
from CastaliaResultParsing import parse_castalia
import pprint


def parse_args():
    parser = argparse.ArgumentParser(description="Analyses Castalia results file")
    parser.add_argument('-i', '--infolder', type=str, required=True,
                        help="Path to folder containing results file(s). This folder and "
                             "all subfolders will be scanned. Multiple files found will be "
                             "combined. Defaults to current directory")
    parser.add_argument('-f', '--file', type=str, required=False, default='results.txt',
                        help="Trace filename to look for - defaults to results.txt")

    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()

    # Add trailing slash if required
    if not args.infolder.endswith('/'):
        args.infolder = '{}/'.format(args.infolder)

    results, histograms = parse_castalia(args.infolder, args.file)

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
    distinct_result_output_names = set()

    for label_and_repeat, modules in results.items():
        for fully_qualified_module, output_names in modules.items():
            for simple_output_name, indexed_values in output_names.items():
                for indexed_output_name, output_value in indexed_values.items():
                    distinct_result_output_names.add((simple_output_name, indexed_output_name))

    pp = pprint.PrettyPrinter(indent=4)
    pp.pprint(distinct_result_output_names)
