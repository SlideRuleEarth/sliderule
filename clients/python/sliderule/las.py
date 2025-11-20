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

import json
import logging
import os

import geopandas
import numpy
import pdal
from pyproj import CRS

from sliderule.session import FatalError

###############################################################################
# GLOBALS
###############################################################################

logger = logging.getLogger(__name__)


###############################################################################
# APIs
###############################################################################

#
# Load LAS/LAZ File into GeoDataFrame
#
def load(path):
    '''
    Load a LAS/LAZ file into a GeoDataFrame using PDAL.

    Parameters
    ----------
        path:               str or os.PathLike
                            path to the LAS/LAZ file to load
    Returns
    -------
    geopandas.GeoDataFrame
        GeoDataFrame containing the LAS/LAZ contents (metadata stored in attrs)
    '''
    path = os.fspath(path)
    arrays, metadata = __read_las_arrays(path)
    return __arrays_to_geodataframe(arrays, metadata, path)


###############################################################################
# INTERNAL APIS
###############################################################################


def __emptyframe(crs):
    geometry = geopandas.points_from_xy([], [], [])
    return geopandas.GeoDataFrame(geometry=geometry, crs=crs)


def __las_spatial_reference(metadata):
    spatial_ref = metadata.get("comp_spatialreference")
    if not spatial_ref:
        raise FatalError("LAS metadata missing comp_spatialreference; only Sliderule LAS files are supported")
    return spatial_ref


def __read_las_arrays(path):
    pipeline_json = json.dumps({"pipeline": [{"type": "readers.las", "filename": path}]})
    pipeline = pdal.Pipeline(pipeline_json)

    try:
        pipeline.execute()
    except RuntimeError as exc:
        raise FatalError(f'failed to load LAS/LAZ file {path}: {exc}') from exc
    arrays = list(pipeline.arrays)
    metadata = __normalize_pdal_metadata(pipeline.metadata)
    return arrays, metadata


def __normalize_pdal_metadata(raw_metadata):
    """
    LAS reader metadata normalization constrained to Sliderule's C++ LAS writer.
    """
    if not isinstance(raw_metadata, dict):
        raise FatalError("unsupported PDAL metadata type; Sliderule LAS output expected")
    parsed = raw_metadata

    metadata_root = parsed.get("metadata", {})
    las_metadata = metadata_root.get("readers.las")
    if not isinstance(las_metadata, dict):
        raise FatalError("missing readers.las metadata; only Sliderule LAS files are supported")

    return las_metadata


def __arrays_to_geodataframe(arrays, metadata, path):
    def attach_las_metadata(gdf):
        gdf.attrs["las_path"] = path
        gdf.attrs["pdal_metadata"] = metadata
        spatial_ref = __las_spatial_reference(metadata)
        if spatial_ref:
            gdf.attrs["las_spatialreference"] = spatial_ref
        vlrs = metadata.get("vlrs")
        if isinstance(vlrs, (list, dict)):
            gdf.attrs["las_vlrs"] = vlrs
        return gdf

    # CRS
    spatial_ref = __las_spatial_reference(metadata)
    crs = CRS.from_user_input(spatial_ref)

    if not arrays:
        gdf = __emptyframe(crs=crs)
        attach_las_metadata(gdf)
        return gdf

    # Merge PDAL arrays
    record = arrays[0] if len(arrays) == 1 else numpy.concatenate(arrays)
    df = geopandas.pd.DataFrame({name: record[name] for name in record.dtype.names})

    # Coordinate fields used by Sliderule's C++ LAS writer
    required_axes = ("X", "Y", "Z")
    missing_axes = [axis for axis in required_axes if axis not in df.columns]
    if missing_axes:
        raise FatalError(f"missing required coordinate fields {missing_axes} in Sliderule LAS file: {path}")

    # Geometry
    geometry = geopandas.points_from_xy(df["X"], df["Y"], df["Z"])

    # Build GeoDataFrame
    gdf = geopandas.GeoDataFrame(df, geometry=geometry, crs=crs)

    # Add height column if needed
    if "height" not in gdf.columns:
        gdf["height"] = geopandas.pd.Series(df["Z"], index=gdf.index)

    attach_las_metadata(gdf)
    return gdf