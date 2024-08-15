import os
from osgeo import gdal, org

# Set GDAL configurations for large cache and multithreading
gdal.SetConfigOption("GDAL_CACHEMAX", "1024")  # 1024 MB cache
gdal.SetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS")  # Use all available CPU cores

def convert_geotiffs_to_cogs(tiffs_dir, cogs_directory):
    # Empty the COGS directory if it exists
    if os.path.exists(cogs_directory):
        for file in os.listdir(cogs_directory):
            os.remove(os.path.join(cogs_directory, file))
    else:
        os.makedirs(cogs_directory)

    # List of GeoTIFF files to convert to COGs
    geotiff_files = [f for f in os.listdir(tiffs_dir) if f.endswith(".tif")]

    # Convert each GeoTIFF to a Cloud-Optimized GeoTIFF
    for geotiff_file in geotiff_files:
        input_path = os.path.join(tiffs_dir, geotiff_file)
        output_cog_path = os.path.join(cogs_directory, f"cog_{geotiff_file}")
        gdal.Translate(
            output_cog_path,
            input_path,
            format="GTiff",
            creationOptions=[
                "TILED=YES",
                "BLOCKXSIZE=256",
                "BLOCKYSIZE=256",
                "COMPRESS=DEFLATE",
                "COPY_SRC_OVERVIEWS=YES",
            ],
        )
        print(f"COG created at: {output_cog_path}")


# Convert original tif files to COGs
# https://www.gebco.net/data_and_products/gridded_bathymetry_data/#area

tifs_directory = "/data/GEBCO/2023/TIFS"
cogs_directory = "/data/GEBCO/2023/COGS"
convert_geotiffs_to_cogs(tifs_directory, cogs_directory)

