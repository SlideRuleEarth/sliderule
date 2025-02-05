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

import xdem
import geopandas as gpd
import sys
import json

# check command line parameters
if len(sys.argv) <= 1:
    print("Not enough parameters: python runner.py <settings json file>")
    sys.exit()

# read settings json
settings_json = sys.argv[1]
with open(settings_json, 'r') as json_file:
    settings = json.load(json_file)

# get settings
fn_epc              = settings.get('fn_epc', "")
fn_dem              = settings.get('fn_dem', "")
fn_reg              = settings.get('fn_reg', "")
coreg_method_str    = settings.get('coreg_method_str', "nuthkaab")
subsample           = settings.get('subsample', 500000)
fit_or_bin          = settings.get('fit_or_bin', "bin_and_fit")

# 1/ INPUTS FROM C++
####################

# 1A/ Inputs from SlideRule to the Python container

# Elevation point cloud provided by SlideRule
# If reading file
epc = gpd.read_file(fn_epc)
# Otherwise directly a geodataframe

# DEM provided by SlideRule
# If reading file
dem = xdem.DEM(fn_dem)
# Otherwise directly an array+geotransform+CRS+nodata
# dem_arr =  # NumPy array
# dem_transform =  # Can be created from a tuple with affine.transform()
# dem_crs =   # Can be created with pyproj.CRS()
# dem_nodata =  # Float
# dem = xdem.DEM.from_array(data=dem_arr, transform=dem_transform, crs=dem_crs, nodata=dem_nodata)

# 1B/ Inputs from user through SlideRule to the Python container

# Mapping coregistration methods
mapping_coreg_methods = {"nuthkaab": xdem.coreg.NuthKaab, "dhmin": xdem.coreg.DhMinimize,
                         "icp": xdem.coreg.ICP, "vshift": xdem.coreg.VerticalShift}

# Mapping potential typed inputs, nested typed dictionary: xdem.coreg.base.InputCoregDict

# Randomization is always mapped to init
init_args = xdem.coreg.base.InRandomDict(subsample=subsample)
# Other arguments are mapped to fit
fit_args = xdem.coreg.base.InFitOrBinDict(fit_or_bin=fit_or_bin)

# 2/ RUN COREGISTRATION
#######################

#
# TEMPORARY work around... z_name needs to be used below (and also set in the parameters above)
#
epc["z"] = epc["h_mean"]

# Fixed parameters to have defined in the container
z_name = "h_mean"  # Name of z variable in the geodataframe
ref = "epc"  # Which data is the reference for the user: the point cloud or DEM?
if ref == "epc":
    reference_elev = epc
    to_be_aligned_elev = dem
else:
    reference_elev = dem
    to_be_aligned_elev = epc

# Run the coregistration
c = mapping_coreg_methods[coreg_method_str](**init_args)
c.fit(reference_elev=reference_elev, to_be_aligned_elev=to_be_aligned_elev, **fit_args)

# 3/ OUTPUTS TO C++
###################

# In any case, extract relevant dictionary of outputs: detailed in xdem.coreg.base.OutputCoregDict
output_random = c.meta["outputs"]["random"]
#output_fitorbin = c.meta["outputs"]["fitorbin"]
output_affine = c.meta["outputs"]["affine"]

# If prefer to transform the elevation data in Python, run apply as well, otherwise do this in C++ later
# coreg_tba_elev = c.apply(elev=to_be_aligned_elev)
# Potentially re-transform DEM object to array+transform+CRS+nodata

# Main outputs for a translation coregistration: shift in X/Y/Z
print(output_affine["shift_x"], output_affine["shift_y"], output_affine["shift_z"])

# Write output
with open(fn_reg, "w") as file:
    file.write(json.dumps(c.meta["outputs"]))