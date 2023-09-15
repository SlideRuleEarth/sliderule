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

import logging
import numpy
import base64
import sliderule
import geopandas
from shapely.geometry import Polygon

###############################################################################
# GLOBALS
###############################################################################

logger = logging.getLogger(__name__)

profiles = {}

###############################################################################
# UTILITIES
###############################################################################

#
# poly2bbox
#
def poly2bbox(poly):
    lats = [p["lat"] for p in poly]
    lons = [p["lon"] for p in poly]
    bbox = [ min(lons), min(lats), max(lons), max(lats) ]
    return bbox

###############################################################################
# APIs
###############################################################################

#
# Sample
#
def sample(asset, coordinates, parms={}):
    '''
    Sample a raster dataset at the coordinates provided

    Parameters
    ----------
    asset:          str
                    data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)
    coordinates:    list
                    list of coordinates as [longitude, latitude]
    parms:          dict
                    dictionary of sampling parameters

    Returns
    -------
    GeoDataFrame
        geolocated sampled values along with metadata

    Examples
    --------
        >>> import sliderule
        >>> gdf = sliderule.sample("arctidcem-mosaic", [[-178.0,51.7]])
    '''
    # Massage Arguments
    if type(coordinates[0]) != list:
        coorindates = [coorindates]

    # Perform Request
    rqst = {"samples": {"asset": asset, **parms}, "coordinates": coordinates}
    rsps = sliderule.source("samples", rqst)

    # Sanity Check Response
    if len(rsps) <= 0:
        return sliderule.emptyframe()

    # Count Records
    num_records = 0
    for input_coord_response in rsps["samples"]:
        for raster_sample in input_coord_response:
            num_records += 1

    # Build Initial Columns
    value_nptype = sliderule.get_definition("rsrec.sample", "value")['nptype']
    columns = {
        'time': numpy.empty(num_records, numpy.int64),
        'latitude': numpy.empty(num_records, numpy.double),
        'longitude': numpy.empty(num_records, numpy.double),
        'file': [],
        'value': numpy.empty(num_records, value_nptype)
    }
    if 'with_flags' in parms:
        flags_nptype    = sliderule.get_definition("rsrec.sample", "flags")['nptype']
        columns         = {
            'flags': numpy.empty(num_records, flags_nptype),
            **columns
        }
    if 'zonal_stats' in parms:
        count_nptype    = sliderule.get_definition("zsrec.sample", "count")['nptype']
        min_nptype      = sliderule.get_definition("zsrec.sample", "min")['nptype']
        max_nptype      = sliderule.get_definition("zsrec.sample", "max")['nptype']
        mean_nptype     = sliderule.get_definition("zsrec.sample", "mean")['nptype']
        median_nptype   = sliderule.get_definition("zsrec.sample", "median")['nptype']
        stdev_nptype    = sliderule.get_definition("zsrec.sample", "stdev")['nptype']
        mad_nptype      = sliderule.get_definition("zsrec.sample", "mad")['nptype']
        columns         = {
            'count': numpy.empty(num_records, count_nptype),
            'min': numpy.empty(num_records, min_nptype),
            'max': numpy.empty(num_records, max_nptype),
            'mean': numpy.empty(num_records, mean_nptype),
            'median': numpy.empty(num_records, median_nptype),
            'stdev': numpy.empty(num_records, stdev_nptype),
            'mad': numpy.empty(num_records, mad_nptype),
            **columns
        }

    # Populate Columns
    i = 0
    per_coord_index = 0
    for input_coord_response in rsps["samples"]:
        longitude = coordinates[per_coord_index][0]
        latitude = coordinates[per_coord_index][1]
        per_coord_index += 1
        for raster_sample in input_coord_response:
            columns['time'][i] = numpy.int64((raster_sample['time'] + 315964800.0) * 1000000000)
            columns['longitude'][i] = numpy.double(longitude)
            columns['latitude'][i] = numpy.double(latitude)
            columns['file'] += raster_sample['file'],
            columns['value'][i] = value_nptype(raster_sample['value'])
            if 'with_flags' in parms:
                columns[i] = flags_nptype(raster_sample['flags'])
            if 'zonal_stats' in parms:
                columns[i] = count_nptype(raster_sample['count'])
                columns[i] = min_nptype(raster_sample['min'])
                columns[i] = max_nptype(raster_sample['max'])
                columns[i] = mean_nptype(raster_sample['mean'])
                columns[i] = median_nptype(raster_sample['median'])
                columns[i] = stdev_nptype(raster_sample['stdev'])
                columns[i] = mad_nptype(raster_sample['mad'])
            i += 1

    # Build GeoDataFrame
    gdf = sliderule.todataframe(columns)
    return gdf

#
# Subset
#
def subset(asset, extents, parms={}):
    '''
    Subset a raster dataset at the extent coordinates provided

    Parameters
    ----------
    asset:          str
                    data source asset (see `Assets </web/rtd/user_guide/ICESat-2.html#assets>`_)
    extents:        list
                    list of extent coordinates as [minimum longitude, minimum latitude, maximum longitude, maximum latitude]
    parms:          dict
                    dictionary of sampling parameters

    Returns
    -------
    list
        list of 2D numpy arrays containing the values of the subsetted raster

    Examples
    --------
        >>> import sliderule
        >>> gdf = sliderule.subset("landsat-hls", [[-179.87, 50.45, -178.77, 50.75]], {"bands": ["B02"]})
    '''
    # Massage Arguments
    if type(extents[0]) != list:
        coorindates = [coorindates]

    # Perform Request
    rqst = {"samples": {"asset": asset, **parms}, "extents": extents}
    rsps = sliderule.source("subsets", rqst)

    # Count Size of Response
    subset_cnt = 0
    for subsets in rsps['subsets']:
        subset_cnt += len(subsets)

    # Initialize Geometry and Columns
    geometry = numpy.empty(subset_cnt, dtype=object)
    columns = {
        "data": numpy.empty(subset_cnt, dtype=object),
        "rows": numpy.empty(subset_cnt, dtype=numpy.uint64),
        "cols": numpy.empty(subset_cnt, dtype=numpy.uint64),
        "time": numpy.empty(subset_cnt, dtype='datetime64[ns]'),
        "file": []
    }

    # Build Geometry and Columns
    row_index = 0
    extent_index = 0
    for subsets in rsps['subsets']:
        extent = extents[extent_index]
        polygon = Polygon([(extent[0], extent[1]), (extent[0], extent[3]), (extent[2], extent[3]), (extent[2], extent[1])])
        for subset in subsets:
            # Add Geometry
            geometry[row_index] = polygon
            # Add Row Elements
            columns["rows"][row_index] = numpy.uint64(subset["rows"])
            columns["cols"][row_index] = numpy.uint64(subset["cols"])
            columns["time"][row_index] = numpy.datetime64(subset["time"], 'ns')
            columns["file"].append(subset["file"])
            data = sliderule.getvalues(base64.b64decode(subset["data"]), subset["datatype"], subset["size"])
            data.shape = (subset["rows"], subset["cols"])
            columns["data"][row_index] = data
            # Goto Next Row
            row_index += 1
        # Goto Next Extent
        extent_index += 1

    # Build and Return GeoDataFrame
    df = geopandas.pd.DataFrame(columns)
    gdf = geopandas.GeoDataFrame(df, geometry=geometry, crs=sliderule.SLIDERULE_EPSG)
    gdf.set_index("time", inplace=True)
    return gdf