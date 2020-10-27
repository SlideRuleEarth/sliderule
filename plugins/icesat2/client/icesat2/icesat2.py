# python

import numpy
import sliderule

###############################################################################
# GLOBALS
###############################################################################

# output dictionary keys
keys = ['segment_id','spot','delta_time','lat','lon','h_mean','dh_fit_dx','dh_fit_dy','rgt','cycle']
# output variable data types
dtypes = ['i','u1','f','f','f','f','f','f','f','u2','u2']

###############################################################################
# UTILITIES
###############################################################################

#
# __recover_h5data
#
def __recover_h5data(rsps):
    """
    rsps: array of responses from engine call to h5 endpoint
    """
    datatype = rsps[0]["datatype"]
    data = ()
    size = 0
    for d in rsps:
        data = data + d["data"]
        size = size + d["size"]
    return sliderule.get_values(data, datatype, size)

#
#  __flatten_atl06
#
def __flatten_atl06(rsps):
    """
    rsps: array of responses from engine call to atl06 endpoint
    """
    global keys, dtypes
    # total length of flattened response
    flatten = numpy.sum([len(r['elevation']) for i,r in enumerate(rsps)]).astype(numpy.int)
    # python dictionary with flattened variables
    flattened = {}
    for key,dtype in zip(keys,dtypes):
        flattened[key] = numpy.zeros((flatten),dtype=dtype)

    # counter variable for flattening responses
    c = 0
    # flatten response
    for i,r in enumerate(rsps):
        for j,v in enumerate(r['elevation']):
            # add each variable
            for key,dtype in zip(keys,dtypes):
                flattened[key][c] = numpy.array(v[key],dtype=dtype)
            # add to counter
            c += 1

    return flattened

###############################################################################
# APIs
###############################################################################

#
#  INIT
#
def init (url, verbose):
    sliderule.set_url(url)
    sliderule.set_verbose(verbose)

#
#  ATL06
#
def atl06 (parm, resource, asset="atl03-cloud", stages=["LSF"], track=0, as_numpy=True):
    # Build ATL06 Request
    rqst = {
        "atl03-asset" : asset,
        "resource": resource,
        "track": track,
        "stages": stages,
        "parms": parm
    }

    # Execute ATL06 Algorithm
    rsps = sliderule.engine("atl06", rqst)

    # Flatten Responses
    if as_numpy:
        rsps = __flatten_atl06(rsps)
    else:
        flattened = {}
        for element in rsps[0]["elevation"][0].keys():
            flattened[element] = [rsps[r]["elevation"][i][element] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
        rsps = flattened

    # Return Responses
    return rsps

#
#  H5
#
def h5 (dataset, resource, asset="atl03-cloud", datatype=sliderule.datatypes["REAL"]):
    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": resource,
        "dataset": dataset,
        "datatype": datatype,
        "id": 0
    }

    # Read H5 File
    rsps = sliderule.engine("h5", rqst)

    # Record Data
    rsps = __recover_h5data(rsps)

    # Return Responses
    return rsps