#!/usr/bin/env python

import plotly
import plotly.graph_objs as graph_objs
from plotly import tools
import os


def plot_monthly(graph_data, station_name, output_path, granularity):
    """
        Plot a single page of solar radiation data for 1 station
        Plots a separate subplot for each month/year, one on top of other in a single column on one page

        graph_data: a dictionary of a station's data
                    - each entry in the dictionary represents a year/month
                    - the value is another dictionary containing graph data lists
                    - usually this will be a list for x values and a list for y values
                    - x values can be datetime objects e.g. datetime(2007,1,1,0,0,0)
                    - y values should be solar radiation

                    For example:
                    {
                        '2007-01': {
                                        'x': [datetime(2007,1,1,0,0,0), datetime(2007,1,1,0,1,0), ...]
                                        'y': [0,0,0,0,0,10,34,123...]
                                    }
                        '2007-02:   ...
                        ...
                    }

        station_name:   the station name, used for labelling graphs and the output file. E.g. 'Lerwick'
        output_path:    where to save the output graph
        granularity:    number of minutes, just used for labelling the output file
    """

    # Initialise the subplot structure
    fig = tools.make_subplots(rows=len(graph_data), cols=1, print_grid=False)

    # Store incrementing row number counter so we know which row to put each successive subplot
    row = 1

    # Calculate the maximum Y (radiation) value over all months, to use as a shared y axis range
    # (can't use plotly's shared_yaxis property because this only applies to subplots on the same row)
    max_y_value = max(y_value for trace_data in graph_data.values() for y_value in trace_data['y'])

    # Plot subplot on one page. Sort the dictionary so we get months in order
    for month_year, trace_data in sorted(graph_data.iteritems()):
        fig.append_trace(
            graph_objs.Scatter(
                x=trace_data['x'],
                y=trace_data['y'],
                name=month_year),
            row=row,  # Row
            col=1  # Column
        )

        row = row + 1

    fig['layout'].update(title="{} ({} minute)".format(station_name, granularity))

    # First save a version without a shared Y axis
    plotly.offline.plot(fig,
                        filename=os.path.join(output_path, "{}-{}-minute-plot.html".format(
                            station_name,
                            granularity)),
                        auto_open=False)

    # Then with a shared Y axis, by applying the max y value as the range for all subplots
    for index in range(1, len(graph_data) + 1):
        fig['layout']['yaxis' + str(index)].update(range=[0, max_y_value])

    fig['layout'].update(title="{} ({} minute, shared y-axis)".format(station_name, granularity))
    plotly.offline.plot(fig,
                        filename=os.path.join(output_path, "{}-{}-minute-shared-y-plot.html".format(
                            station_name,
                            granularity)),
                        auto_open=False)
