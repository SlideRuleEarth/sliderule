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

import time
import numpy
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
    format that is easy to use in Python.  The compromise that this function takes is that it allows the user to supply the desired data type of the
    returned data via the **datatype** parameter, and the function will then return a **numpy** array of values with that data type.

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
                    the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
        startrow:   int
                    the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
        numrows:    int
                    the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.

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

    For a full discussion of the data type conversion options, see `h5 </web/rtd/api_reference/h5.html#h5>`_.

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
        * "col" (int): the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
        * "startrow" (int): the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
        * "numrows" (int): the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.

    Examples
    --------
        >>> from sliderule import icesat2
        >>> icesat2.init(["127.0.0.1"], False)
        >>> datasets = [
        ...         {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
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
        rsps = sliderule.source("h5p", rqst, stream=True)
    except RuntimeError as e:
        logger.critical(e)
        rsps = []

    # Build Record Data
    results = {}
    for result in rsps:
        results[result["dataset"]] = sliderule.getvalues(result["data"], result["datatype"], result["size"])

    # Return Results
    return results
