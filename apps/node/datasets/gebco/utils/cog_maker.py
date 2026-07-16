import os
import sys
from osgeo import gdal

# Enable GDAL exceptions to avoid future warnings
gdal.UseExceptions()

# Set GDAL configurations for large cache and multithreading
gdal.SetConfigOption("GDAL_CACHEMAX", "1024")  # 1024 MB cache
gdal.SetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS")  # Use all available CPU cores

def convert_geotiffs_to_cogs(year, tiffs_dir, cogs_dir):
    # Empty the COGS directory if it exists
    if os.path.exists(cogs_dir):
        for file in os.listdir(cogs_dir):
            os.remove(os.path.join(cogs_dir, file))
    else:
        os.makedirs(cogs_dir)

    # List of GeoTIFF files to convert to COGs
    geotiff_files = [f for f in os.listdir(tiffs_dir) if f.endswith(".tif")]

    # tif files have year in their name, example of 2023 tif file:
    # gebco_2023_sub_ice_n0.0_s-90.0_w-180.0_e-90.0.tif
    # Check if the year is part of the file name
    for geotiff_file in geotiff_files:
        if year not in geotiff_file:
            print(f"Error: File '{geotiff_file}' does not contain the year '{year}' in its name.")
            sys.exit(1)

    # Convert each GeoTIFF to a Cloud-Optimized GeoTIFF
    for geotiff_file in geotiff_files:
        input_path = os.path.join(tiffs_dir, geotiff_file)
        output_cog_path = os.path.join(cogs_dir, f"cog_{geotiff_file}")
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

if __name__ == "__main__":
    # List of supported years
    supported_years = ["2023", "2024"]

    if len(sys.argv) != 2:
        print("Usage: python cog_maker.py <year>")
        sys.exit(1)

    # Get the year from the command line argument
    year = sys.argv[1]

    # Ensure the provided year is valid
    if year not in supported_years:
        print(f"Error: Invalid year. Supported years are: {', '.join(supported_years)}")
        sys.exit(1)

    # Compose directory paths based on the year
    tifs_dir = f"/data/GEBCO/{year}/TIFS"
    cogs_dir = f"/data/GEBCO/{year}/COGS"

    # Check if the directories exist
    if not os.path.exists(tifs_dir):
        print(f"Error: Directory '{tifs_dir}' does not exist.")
        sys.exit(1)

    if not os.path.exists(cogs_dir):
        print(f"Error: Directory '{cogs_dir}' does not exist.")
        sys.exit(1)

    # Call the conversion function
    convert_geotiffs_to_cogs(year, tifs_dir, cogs_dir)