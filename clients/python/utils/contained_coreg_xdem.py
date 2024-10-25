import xdem
import geopandas as gpd

# Example files and user input
fn_epc = ""
fn_dem = ""

coreg_method_str = "nuthkaab"
subsample = 500000
fit_or_bin = "bin_and_fit"

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

# Fixed parameters to have defined in the container
z_name = "h_li"  # Name of z variable in the geodataframe
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
output_fitorbin = c.meta["outputs"]["fitorbin"]
output_affine = c.meta["outputs"]["affine"]

# If prefer to transform the elevation data in Python, run apply as well, otherwise do this in C++ later
# coreg_tba_elev = c.apply(elev=to_be_aligned_elev)
# Potentially re-transform DEM object to array+transform+CRS+nodata

# Main outputs for a translation coregistration: shift in X/Y/Z
print(output_affine["shift_x"], output_affine["shift_y"], output_affine["shift_z"])
