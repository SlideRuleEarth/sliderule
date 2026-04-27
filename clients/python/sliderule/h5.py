# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import numpy
import tempfile
import sliderule
from sliderule import logger

###############################################################################
# GLOBALS
###############################################################################

DATATYPES = {
    "TEXT":     0,
    "REAL":     1,
    "INTEGER":  2,
    "DYNAMIC":  3
}

ALL_ROWS = -1
ALL_COLS = -1

###############################################################################
# APIs
###############################################################################

#
#  H5
#
def h5 (dataset, resource, asset, datatype=DATATYPES["DYNAMIC"], col=0, startrow=0, numrows=ALL_ROWS):
    '''
    Reads a dataset from an HDF5 file and returns the values of the dataset in a list

    This function provides an easy way for locally run scripts to get direct access to HDF5 data stored in a cloud environment.
    But it should be noted that this method is not the most efficient way to access remote H5 data, as the data is accessed one dataset at a time.
    The ``h5p`` api is the preferred solution for reading multiple datasets.

    One of the difficulties in reading HDF5 data directly from a Python script is converting the format of the data as it is stored in HDF5 to a data
    format that is easy to use in Python.  The compromise that this function takes is that it allows the user to supply the override the data type of the
    returned data via the **datatype** parameter, and the function will then return a **numpy** array of values with that data type.  If the **datatype**
    parameter is not supplied, then the code does its best to match the HDF5 type to the corresponding Python type.

    The data type is supplied as a ``DATATYPES`` enumeration:

    - ``DATATYPES["TEXT"]``: return the data as a string of unconverted bytes
    - ``DATATYPES["INTEGER"]``: return the data as an array of integers
    - ``DATATYPES["REAL"]``: return the data as an array of double precision floating point numbers
    - ``DATATYPES["DYNAMIC"]``: return the data in the numpy data type that is the closest match to the data as it is stored in the HDF5 file

    Parameters
    ----------
        dataset:    str
                    full path to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``)
        resource:   str
                    HDF5 filename
        asset:      str
                    data source asset
        datatype:   int
                    the type of data the returned dataset list should be in (datasets that are naturally of a different type undergo a best effort conversion to the specified data type before being returned)
        col:        int
                    the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned. Ignored if ``slice`` is supplied. If the variable has more than one column, then by default the first column is read, if all columns are wanted, then set col=-1 and the result will be a flattened array of all of the data.
        startrow:   int
                    the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension). Ignored if ``slice`` is supplied.
        numrows:    int
                    the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset. Ignored if ``slice`` is supplied.

    Returns
    -------
    numpy array
        dataset values

    Examples
    --------
        >>> segments    = h5.h5("/gt1r/land_ice_segments/segment_id",  resource, asset)
        >>> heights     = h5.h5("/gt1r/land_ice_segments/h_li",        resource, asset)
        >>> latitudes   = h5.h5("/gt1r/land_ice_segments/latitude",    resource, asset)
        >>> longitudes  = h5.h5("/gt1r/land_ice_segments/longitude",   resource, asset)
        >>> df = pd.DataFrame(data=list(zip(heights, latitudes, longitudes)), index=segments, columns=["h_mean", "latitude", "longitude"])
    '''
    datasets = [ { "dataset": dataset, "datatype": datatype, "col": col, "startrow": startrow, "numrows": numrows } ]
    values = h5p(datasets, resource, asset=asset)
    if len(values) > 0:
        return values[dataset]
    else:
        return numpy.empty(0)

#
#  Parallel H5
#
def h5p (datasets, resource, asset):
    '''
    Reads a list of datasets from an HDF5 file and returns the values of the dataset in a dictionary of lists.

    This function is considerably faster than the ``icesat2.h5`` function in that it not only reads the datasets in
    parallel on the server side, but also shares a file context between the reads so that portions of the file that
    need to be read multiple times do not result in multiple requests to S3.

    For a full discussion of the data type conversion options, see `h5 </api_reference/h5.html#h5>`_.

    The “slice” parameter for a dataset allows the user to supply a range for each dimension and then flatten the result.
    For example, in the case of a 2D variable, the slice is effectively [[start_row, end_row], [start_column, end_column]].
    If all data across all dimensions is wanted, the slice becomes [[0,-1]].

    Parameters
    ----------
        datasets:   dict
                    list of full paths to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``); see below for additional parameters that can be added to each dataset
        resource:   str
                    HDF5 filename
        asset:      str
                    data source asset

    Returns
    -------
    dict
        numpy arrays of dataset values, where the keys are the dataset names

        The `datasets` dictionary can optionally contain the following elements per entry:

        * "valtype" (int): the type of data the returned dataset list should be in (datasets that are naturally of a different type undergo a best effort conversion to the specified data type before being returned)
        * "slice" (list): optional multi-dimensional slice, expressed as ``[[start0, end0], [start1, end1], ...]`` (half-open ``[start, end)``, ``-1`` means to the end). Up to ``H5Coro::MAX_NDIMS`` dimensions are honored; missing trailing dimensions default to ``[0, end]``. If "slice" is provided, "col"/"startrow"/"numrows" are ignored.
        * "col" (int): the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
        * "startrow" (int): the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
        * "numrows" (int): the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init(["127.0.0.1"], False)
        >>> datasets = [
        ...         {"dataset": "/gt1l/land_ice_segments/h_li", "slice": [[0, 5], [0, -1]]},  # slice rows 0-4, all cols
        ...         {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},                # legacy window
        ...         {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5}
        ...     ]
        >>> rsps = h5.h5p(datasets, "ATL06_20181019065445_03150111_003_01.h5", "atlas-local")
        >>> print(rsps)
        {'/gt2r/land_ice_segments/h_li': array([45.3146427 , 45.27640582, 45.23608027, 45.21131015, 45.15692304]),
         '/gt2l/land_ice_segments/h_li': array([45.35118977, 45.33535027, 45.27195617, 45.21816889, 45.18534204]),
         '/gt1l/land_ice_segments/h_li': array([45.68811156, 45.71368944, 45.74234326, 45.74614113, 45.79866465]),
         '/gt3l/land_ice_segments/h_li': array([45.29602321, 45.34764226, 45.31430979, 45.31471701, 45.30034622]),
         '/gt1r/land_ice_segments/h_li': array([45.72632446, 45.76512574, 45.76337375, 45.77102473, 45.81307948]),
         '/gt3r/land_ice_segments/h_li': array([45.14954134, 45.18970635, 45.16637644, 45.15235916, 45.17135806])}
    '''
    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": resource,
        "datasets": datasets,
    }

    # Read H5 File
    try:
        rsps = sliderule.source("h5p", rqst, stream=True, rethrow=True)
    except RuntimeError as e:
        logger.critical(e)
        rsps = []

    # Build Record Data
    results = {}
    for result in rsps:
        results[result["dataset"]] = sliderule.getvalues(result["data"], result["datatype"], result["size"])

    # Return Results
    return results

#
#  DataFrame H5
#
def h5x (variables, resource, asset, groups=None, col=None, startrow=None, numrows=None, index_column=None, time_column=None, x_column=None, y_column=None, z_column=None, crs=None, session=None):
    '''
    Builds a DataFrame from an HDF5 file where each variable in ``variables`` is a column.

    The ``groups`` parameter is used to create datasets from multiple groups within the file.  For example,
    if ``groups = ["/data/r1", "/data/r2", "/data/r3"]`` and ``datasets = ["x", "y"]``
    then the dataframe will have two columns: "x" and "y"
    that are populated from six datasets within the file: "/data/r1/x", "/data/r2/x", "data/r3/x", and "/data/r1/y", "/data/r2/y", "data/r3/y"

    Parameters
    ----------
        variables:  list
                    list of variables to read from each group
        resource:   str
                    HDF5 filename
        asset:      str
                    data source asset
        groups:     list
                    list of full paths to the groups to read from the file

    Returns
    -------
    DataFrame
        A pandas dataframe of the data read from the file
    '''
    # defaults
    if groups == None:
        groups = ["/"]

    # baseline parameters
    parms = {
        "variables": variables,
        "groups": groups,
    }

    # optional parameters
    if col: parms["col"] = col
    if startrow: parms["startrow"] = startrow
    if numrows: parms["numrows"] = numrows
    if index_column: parms["index"] = index_column
    if time_column: parms["time"] = time_column
    if x_column: parms["x"] = x_column
    if y_column: parms["y"] = y_column
    if z_column: parms["z"] = z_column
    if crs: parms["crs"] = crs

    # determine format
    format = "parquet"
    if x_column and y_column:
        format = "geoparquet"

    # manage output for convenience
    delete_tempfile = False
    if "output" not in parms:
        delete_tempfile = True
        parms["output"] = {
            "path": tempfile.mktemp(),
            "format": format,
            "open_on_complete": True
        }

    # build request
    rqst = {
        "parms": parms,
        "asset": asset,
        "resource": resource
    }

    # make request
    rsps = sliderule.source("h5x", rqst, stream=True, session=session)

    # build geodataframe
    gdf = sliderule.procoutputfile(parms, rsps)

    # delete tempfile
    if delete_tempfile and os.path.exists(parms["output"]["path"]):
        os.remove(parms["output"]["path"])

    # return geodataframe
    return gdf
