import sys
import warnings
import numpy
import geopandas
import uuid
import base64
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
from shapely.geometry.multipolygon import MultiPolygon
from shapely.geometry import Polygon
from sklearn.cluster import KMeans
import sliderule
from sliderule import version

#
# GeoDataFrame to Polygon
#
def __gdf2poly(gdf):
    # pull out coordinates
    hull = gdf.unary_union.convex_hull
    polygon = [{"lon": coord[0], "lat": coord[1]} for coord in list(hull.exterior.coords)]

    # determine winding of polygon #
    #              (x2               -    x1)             *    (y2               +    y1)
    wind = sum([(polygon[i+1]["lon"] - polygon[i]["lon"]) * (polygon[i+1]["lat"] + polygon[i]["lat"]) for i in range(len(polygon) - 1)])
    if wind > 0:
        # reverse direction (make counter-clockwise) #
        ccw_poly = []
        for i in range(len(polygon), 0, -1):
            ccw_poly.append(polygon[i - 1])
        # replace region with counter-clockwise version #
        polygon = ccw_poly

    # return polygon
    return polygon

#
# MAIN
#
if __name__ == '__main__':

    source = sys.argv[1]

    cellsize = 0.01
    tolerance = 0.0
    n_clusters = 1

    # create geodataframe
    gdf = geopandas.read_file(source)

    # create input driver
    if (source.find(".geojson") > 1):
        inp_driver = ogr.GetDriverByName('GeoJSON')
    else: # (source.find(".shp") > 1)
        inp_driver = ogr.GetDriverByName('ESRI Shapefile')

    # create input layer
    inp_source = inp_driver.Open(source, 0)
    inp_lyr = inp_source.GetLayer()

    # get extent of raster
    x_min, x_max, y_min, y_max = inp_lyr.GetExtent()
    x_ncells = int((x_max - x_min) / cellsize)
    y_ncells = int((y_max - y_min) / cellsize)

    print("Original box for raster create")
    print( "x_min: ", x_min, "x_max: ", x_max, "\ny_min: ", y_min, "y_max: ", y_max, "\ncellsize: ", cellsize, "x_ncells: ", x_ncells, "y_ncells: ", y_ncells, "\n")

    EPSG_POLAR_NORTH   = "EPSG:3413"  # WGS 84 / NSIDC Sea Ice Polar Stereographic North
    EPSG_POLAR_SOUTH   = "EPSG:3976"  # WGS 84 / NSIDC Sea Ice Polar Stereographic South
    EPGS_MERCATOR      = "EPSG:4326"  # WGS 84 / Mercator, Earth as Geoid, Coordinate system on the surface of a sphere or ellipsoid of reference. 
    EPSG_WEB_MERCATOR  = "EPSG:3857"  # WGS 84 / Web Mercator (Pseudo Mercator), Earth as perfec shpere, Coordinate system PROJECTED from the surface of the sphere.
                                      # used by Google Maps, Mapquest, etc

    EPSG_PLATE_CARTE  = "EPSG:32663" # WGS 84 / World Equidistant Cylindrical, meters, Server default for non polar regions

    # setup raster output
    out_driver = gdal.GetDriverByName('GTiff')
    out_filename = '/vsimem/' + str(uuid.uuid4())
    out_source = out_driver.Create(out_filename, x_ncells, y_ncells, 1, gdal.GDT_Byte, options = [ 'COMPRESS=DEFLATE' ])
    out_source.SetGeoTransform((x_min, cellsize, 0, y_max, 0, -cellsize))
    out_source.SetProjection(inp_lyr.GetSpatialRef().ExportToWkt())
    out_lyr = out_source.GetRasterBand(1)
    out_lyr.SetNoDataValue(200)

    # rasterize
    gdal.RasterizeLayer(out_source, [1], inp_lyr, burn_values=[1])

    # close the data sources
    inp_source = None
    rast_ogr_ds = None
    out_source = None
    
    # read out reprojected raster data
    f = gdal.VSIFOpenL(out_filename, 'rb')
    gdal.VSIFSeekL(f, 0, 2)  # seek to end
    size = gdal.VSIFTellL(f)
    gdal.VSIFSeekL(f, 0, 0)  # seek to beginning
    raster = gdal.VSIFReadL(1, size, f)
    gdal.VSIFCloseL(f)

    with open("tmp.tiff", "wb") as file:
        file.write(raster)


    src = gdal.Open(out_filename)
    ulx, xres, xskew, uly, yskew, yres = src.GetGeoTransform()
    lrx = ulx + (src.RasterXSize * xres)
    lry = uly + (src.RasterYSize * yres)

    x_min = ulx
    x_max = lrx
    y_min = lry
    y_max = uly

    x_ncells = src.RasterXSize
    y_ncells = src.RasterYSize

    print("After raster create")
    print( "x_min: ", x_min, "x_max: ", x_max, "\ny_min: ", y_min, "y_max: ", y_max, "\nXcellsize: ", xres, "Ycellsize: ", yres, "x_ncells: ", x_ncells, "y_ncells: ", y_ncells, "\n")
    
    
    src = None 
    new_out_filename = out_filename + 'reprojected'
    gdal.Warp(new_out_filename, out_filename, dstSRS = EPSG_POLAR_NORTH) # North

    #read out reprojected raster data
    f = gdal.VSIFOpenL(new_out_filename, 'rb')
    gdal.VSIFSeekL(f, 0, 2)  # seek to end
    size = gdal.VSIFTellL(f)
    gdal.VSIFSeekL(f, 0, 0)  # seek to beginning
    raster = gdal.VSIFReadL(1, size, f)
    gdal.VSIFCloseL(f)

    with open("tmp_polar.tiff", "wb") as file:
        file.write(raster)

    src = gdal.Open(new_out_filename)
    ulx, xres, xskew, uly, yskew, yres = src.GetGeoTransform()
    lrx = ulx + (src.RasterXSize * xres)
    lry = uly + (src.RasterYSize * yres)

    x_min = ulx
    x_max = lrx
    y_min = lry
    y_max = uly

    x_ncells = src.RasterXSize
    y_ncells = src.RasterYSize

    print("After raster projection")
    print( "x_min: ", x_min, "x_max: ", x_max, "\ny_min: ", y_min, "y_max: ", y_max, "\nXcellsize: ", xres, "Ycellsize: ", yres, "x_ncells: ", x_ncells, "y_ncells: ", y_ncells, "\n")
    
    sys.exit()






    # simplify polygon
    if(tolerance > 0.0):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            gdf = gdf.buffer(tolerance)
            gdf = gdf.simplify(tolerance)

    # generate polygon
    polygon = __gdf2poly(gdf)
    
    # generate clusters
    clusters = []
    if n_clusters > 1:
        # pull out centroids of each geometry object
        if "CenLon" in gdf and "CenLat" in gdf:
            X = numpy.column_stack((gdf["CenLon"],gdf["CenLat"]))
        else:
            s = gdf.centroid
            X = numpy.column_stack((s.x, s.y))
        # run k means clustering algorithm against polygons in gdf
        kmeans = KMeans(n_clusters=n_clusters, init='k-means++', random_state=5, max_iter=400)
        y_kmeans = kmeans.fit_predict(X)
        k = geopandas.pd.DataFrame(y_kmeans, columns=['cluster'])
        gdf = gdf.join(k)
        # build polygon for each cluster
        for n in range(n_clusters):
            c_gdf = gdf[gdf["cluster"] == n]
            c_poly = __gdf2poly(c_gdf)
            clusters.append(c_poly)

    # encode image in base64
    b64image = base64.b64encode(raster).decode('UTF-8')

    # return region #
    print({
        "gdf": gdf,
        "poly": polygon, # convex hull of polygons
        "clusters": clusters, # list of polygon clusters for cmr request
        "raster": {
            "image": b64image, # geotiff image
            "imagelength": len(b64image), # encoded image size of geotiff
            "dimension": (y_ncells, x_ncells), # rows x cols
            "bbox": (x_min, y_min, x_max, y_max), # lon1, lat1 x lon2, lat2
            "cellsize": cellsize # in degrees
        }
    })
