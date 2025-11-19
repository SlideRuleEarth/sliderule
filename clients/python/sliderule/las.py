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
    if not isinstance(metadata, dict):
        return None
    for key in ["comp_spatialreference", "spatialreference"]:
        value = metadata.get(key)
        if value:
            return value
    return None


def __read_las_arrays(path):
    pipeline_json = json.dumps({"pipeline": [{"type": "readers.las", "filename": path}]})
    pipeline = pdal.Pipeline(pipeline_json)

    try:
        if hasattr(pipeline, "validate"):  # Newer PDAL versions support validate()
            pipeline.validate()            # PDAL docs recommend calling validate() before execute() even on hardcoded pipelines
        pipeline.execute()
    except RuntimeError as exc:
        raise FatalError(f'failed to load LAS/LAZ file {path}: {exc}') from exc
    arrays = list(pipeline.arrays)
    metadata = __normalize_pdal_metadata(pipeline.metadata)
    return arrays, metadata


def __normalize_pdal_metadata(raw_metadata):
    """
    Normalize PDAL metadata specifically for LAS readers.
    PDAL metadata returned from pipeline.metadata typically has this structure:

    {
        "metadata": {
            "readers.las": {
                ... actual useful LAS metadata (point count, bounds, scale, offsets, etc) ...
            }
        }
    }

    However, the input may also come as:
    - bytes or JSON string (needs decoding and parsing)
    - a flat dict that already contains 'metadata'
    - malformed, missing, or unusable types

    This function:
    1. Decodes bytes → string → JSON dict when needed
    2. Navigates into the "metadata" → "readers.las" block
    3. Returns only the LAS reader section (dict) if found
    4. Safely falls back to {} for any invalid or unexpected input
    """

    # Convert bytes to string
    if isinstance(raw_metadata, (bytes, bytearray)):
        try:
            raw_metadata = raw_metadata.decode('utf-8')
        except Exception:
            return {}

    # Convert JSON string to dict
    if isinstance(raw_metadata, str):
        try:
            raw_metadata = json.loads(raw_metadata)
        except Exception as exc:
            logger.debug(f"Unable to parse PDAL metadata JSON: {exc}")
            return {}

    # Expect a dict from here
    if not isinstance(raw_metadata, dict):
        return {}

    # Drill into 'metadata' → 'readers.las'
    metadata_root = raw_metadata.get("metadata", raw_metadata)
    readers_las = metadata_root.get("readers.las")

    return readers_las if isinstance(readers_las, dict) else {}


def __select_column(columns, candidates):
    for candidate in candidates:
        cand_lower = candidate.lower()
        for column in columns:
            if column.lower() == cand_lower:
                return column
    return None


def __arrays_to_geodataframe(arrays, metadata, path):
    # Helper to attach LAS metadata to any GeoDataFrame
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

    # Coordinate fields
    x_col = __select_column(df.columns, ["X", "longitude", "lon"])
    y_col = __select_column(df.columns, ["Y", "latitude", "lat"])
    z_col = __select_column(df.columns, ["Z", "height", "h"])

    if not (x_col and y_col and z_col):
        raise FatalError(f"missing coordinate fields in LAS/LAZ file: {path}")

    # Geometry
    geometry = geopandas.points_from_xy(df[x_col], df[y_col], df[z_col])

    # Build GeoDataFrame
    gdf = geopandas.GeoDataFrame(df, geometry=geometry, crs=crs)

    # Add height column if needed
    if "height" not in gdf.columns and z_col.lower() != "height":
        gdf["height"] = geopandas.pd.Series(df[z_col], index=gdf.index)

    attach_las_metadata(gdf)
    return gdf
