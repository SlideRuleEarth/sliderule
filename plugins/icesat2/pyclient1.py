# Imports

import sys
import requests
import json

import pandas as pd
import numpy as np

import matplotlib.pyplot as plt
from ipywidgets import interact
from bokeh.io import push_notebook, show, output_notebook
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource

#
# Parameters
#
server_url = 'http://127.0.0.1:9081'

filename = "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5"
dataset = "gt2l/heights/dist_ph_along"
datatag = 0

rqst_dict = {
        "filename": filename,
        "dataset": dataset,
        "id": datatag
    }

#
# SlideRule API
#
def engine (api, parm):
    url = '%s/engine/%s' % (server_url, api)
    return requests.post(url, data=parm, stream=True)

#
# Default Matplotlib Plot of Dataframe Column
#
def dfplot(df, col, cond_col="", cond_val=0):
    if cond_col != "":
        c = df[df[cond_col] == cond_val]
        s = c[pd.isnull(c[col]) == False][col]
    else:
        s = df[pd.isnull(df[col]) == False][col]
    s.plot(figsize=(15,5))
    plt.title(col)
    plt.show()

#
# Sum of Dataframe Column
#
def dfsum(df, col, cond_col="", cond_val=0):
    if cond_col != "":
        total = df[df[cond_col] == cond_val][col].sum()
    else:
        total = df[col].sum()
    return total

#
# Matplotlib Histogram of Value Counts in Dataframe Column
#
def dfplotvc(df, col, cond_col="", cond_val=0):
    if cond_col != "":
        vc = df[df[cond_col] == cond_val][col].value_counts()
    else:
        vc = df[col].value_counts()
    plt.clf()
    plt.figure(figsize=(15,8))
    vc.plot(kind='bar')
    plt.show()

#
# Interactive Bokeh Plot of Dataframe Column
# ... note that x axis is a recreated list of item number
# ... and does not reflect original index in dataframe
#
def dfbokeh(df, col, cond_col="", cond_val=0):
    p = figure(title=col, plot_height=300, plot_width=600)
    if cond_col != "":
        c = df[df[cond_col] == cond_val]
        s = c[pd.isnull(c[col]) == False][col]
    else:
        s = df[pd.isnull(df[col]) == False][col]
    y = s.values
    x = [i for i in range(len(y))]
    r = p.line(x, y, color="#2222aa", line_width=3)
    show(p, notebook_handle=True)


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line
    if len(sys.argv) > 1:
        server_url = sys.argv[1]

    # Make API Request
    d = engine("h5", json.dumps(rqst_dict))

    # Read Response
    data_array = np.empty(1)
    for line in d.iter_lines():
        num_elem = int(len(line) / 4)
        print(num_elem)
        val_array = np.frombuffer(line, dtype=np.float32, count=num_elem)
        data_array = np.append(data_array, val_array)
        break

    # Build DataFrame
    df = pd.DataFrame(data=data_array, index=[i for i in range(len(data_array))], columns=[dataset])

    # Display Results
    dfbokeh(df, dataset)
