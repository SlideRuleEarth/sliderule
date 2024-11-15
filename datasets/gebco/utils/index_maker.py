import os
import sys
from osgeo import gdal, ogr

# Enable GDAL exceptions to avoid future warnings
gdal.UseExceptions()

def create_geojson(dir_path, index_file):
    if not os.path.exists(dir_path):
        raise ValueError(f"The directory '{dir_path}' does not exist.")

    # Find all .tif files in the directory
    tif_files = [os.path.join(dir_path, f) for f in os.listdir(dir_path) if f.endswith(".tif")]

    if not tif_files:
        raise ValueError(f"No .tif files found in directory: {dir_path}")

    # Set the output path for the geojson file in the same directory
    geojson_output_path = os.path.join(dir_path, index_file)

    # Remove if it already exists
    if os.path.exists(geojson_output_path):
        os.remove(geojson_output_path)

    # Create a GeoJSON layer
    driver = ogr.GetDriverByName("GeoJSON")
    geojson_ds = driver.CreateDataSource(geojson_output_path)
    geojson_layer = geojson_ds.CreateLayer("cog_geometries", geom_type=ogr.wkbPolygon)

    # Create an "id" field in the GeoJSON layer
    field_id = ogr.FieldDefn("id", ogr.OFTString)
    geojson_layer.CreateField(field_id)

    # Create an "data_raster" field in the GeoJSON layer
    data_raster = ogr.FieldDefn("data_raster", ogr.OFTString)
    geojson_layer.CreateField(data_raster)

    # Create an "flags_raster" field in the GeoJSON layer
    flags_raster = ogr.FieldDefn("flags_raster", ogr.OFTString)
    geojson_layer.CreateField(flags_raster)

    # Create an "daterime" field in the GeoJSON layer
    datetime = ogr.FieldDefn("datetime", ogr.OFTString)
    geojson_layer.CreateField(datetime)

    # Add geometry from each tif to the GeoJSON layer
    id = 1
    for tif_file in tif_files:
        raster_ds = gdal.Open(tif_file)

        if '_tid_' in tif_file:
            continue

        # print(f"Processed: {tif_file}")

         # Get the geotransform and size of the raster
        geotransform = raster_ds.GetGeoTransform()
        x_size = raster_ds.RasterXSize
        y_size = raster_ds.RasterYSize

        # Calculate the corners of the raster
        top_left_x = geotransform[0]
        top_left_y = geotransform[3]
        bottom_right_x = top_left_x + geotransform[1] * x_size
        bottom_right_y = top_left_y + geotransform[5] * y_size

        # Create a polygon to represent the raster's bounding box
        ring = ogr.Geometry(ogr.wkbLinearRing)
        ring.AddPoint(top_left_x, top_left_y)  # Top-left corner
        ring.AddPoint(bottom_right_x, top_left_y)  # Top-right corner
        ring.AddPoint(bottom_right_x, bottom_right_y)  # Bottom-right corner
        ring.AddPoint(top_left_x, bottom_right_y)  # Bottom-left corner
        ring.AddPoint(top_left_x, top_left_y)  # Closing the ring

        polygon = ogr.Geometry(ogr.wkbPolygon)
        polygon.AddGeometry(ring)

        # Create a new feature and set the geometry
        feature = ogr.Feature(geojson_layer.GetLayerDefn())
        feature.SetGeometry(polygon)

        # Set the "id" attribute to the index of the raster
        feature.SetField("id", id)
        id += 1

        # Set the "datetime" attribute to the middle of the specified year
        middle_of_year = f"{year}-07-02T00:00:00Z"
        feature.SetField("datetime", middle_of_year)

        # Remove the directory path from the tif file name
        tif_file = os.path.basename(tif_file)

        # Set the "data_raster" attribute to the raster name
        feature.SetField("data_raster", tif_file)

        # Create flags_raster file name by replacing '_sub_ice_' with '_tid_' from tif_file
        flags_file = tif_file.replace("_sub_ice_", "_tid_")

        # Set the "flags_raster" attribute to the flags_raster file name
        feature.SetField("flags_raster", flags_file)

        geojson_layer.CreateFeature(feature)  # Add the feature to the GeoJSON layer
        feature.Destroy()


    geojson_ds.Destroy()  # Close and save the GeoJSON file
    print(f"GeoJSON index file created at: {geojson_output_path}")


if __name__ == "__main__":
    # List of supported years
    supported_years = ["2023", "2024"]

    if len(sys.argv) != 2:
        print("Usage: python index_maker.py <year>")
        sys.exit(1)

    # Get the year from the command line argument
    year = sys.argv[1]

    # Ensure the provided year is valid
    if year not in supported_years:
        print(f"Error: Invalid year. Supported years are: {', '.join(supported_years)}")
        sys.exit(1)

    # Compose directory paths based on the year
    cogs_dir = f"/data/GEBCO/{year}/COGS"
    index_file = "index.geojson"

    create_geojson(cogs_dir, index_file)
