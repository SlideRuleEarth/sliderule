#
# Requests SlideRule to process the provided region of interest twice:
# (1) perform ATL06-SR algorithm to calculate elevations in region
# (2) retrieve photon counts from each ATL06 extent
#
# The region of interest is passed in as a json file that describes the
# geospatial polygon for the region. An example json object is below:
#
# {
#   "region": [{"lon": 86.269733430535638, "lat": 28.015965655545852},
#              {"lon": 86.261403224371804, "lat": 27.938668666352985},
#              {"lon": 86.302412923741514, "lat": 27.849318271202186},
#              {"lon": 86.269733430535638, "lat": 28.015965655545852}]
# }
#

import time
from sliderule import icesat2

from osgeo import osr, ogr, gdal




###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = "127.0.0.1"
    asset = "atlas-local"
    resource = "ATL03_20181017222812_02950102_005_01.h5"

    # Region of Interest #
    region = icesat2.toregion("examples/grandmesa.geojson")

    # Configure SlideRule #
    icesat2.init(url, True)

    # Build ATL06 Request #
    parms = {
        "poly": region["poly"],
        "raster": region["raster"],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
    }

    # Latch Start Time
    perf_start = time.perf_counter()

    # Request ATL06 Data
    print("before ====> icesat2.atl03s()")
    gdf = icesat2.atl03s(parms, resource, asset)
    print("after  ====> icesat2.atl03s()")

    # Latch Stop Time
    perf_stop = time.perf_counter()

    # Build DataFrame of SlideRule Responses
    num_photons = len(gdf)

    # Display Statistics
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    if num_photons > 0:
        print("Reference Ground Tracks: {}".format(gdf["rgt"].unique()))
        print("Cycles: {}".format(gdf["cycle"].unique()))
        print("Received {} photons".format(num_photons))
    else:
        print("No photons were returned")


    source = osr.SpatialReference()
    source.ImportFromEPSG(4326)
    
    dest_plate_carte1 = osr.SpatialReference()
    dest_plate_carte1.ImportFromEPSG(32662)
    
    dest_plate_carte2 = osr.SpatialReference()
    dest_plate_carte2.ImportFromEPSG(32663)

    dest_npolar= osr.SpatialReference()
    dest_npolar.ImportFromEPSG(3995)
    
    dest_spolar= osr.SpatialReference()
    dest_spolar.ImportFromEPSG(3031)


    lon = -100.0
    lat = 30.0
    
    transform = osr.CoordinateTransformation(source, dest_plate_carte1)
    point = ogr.Geometry(ogr.wkbPoint)
    point.AddPoint(lat, lon)
    point.Transform(transform)
    print("PCARTE1 (lon, lat)", lon, lat, point.GetX(), point.GetY())
    transform = point = None 



    transform = osr.CoordinateTransformation(source, dest_plate_carte2)
    point = ogr.Geometry(ogr.wkbPoint)
    point.AddPoint(lat, lon)
    point.Transform(transform)
    print("PCARTE2 (lon, lat)", lon, lat, point.GetX(), point.GetY())
    transform = point = None 
    

    lon = -100.0
    lat = 80.0
    transform = osr.CoordinateTransformation(source, dest_npolar)
    point = ogr.Geometry(ogr.wkbPoint)
    point.AddPoint(lat, lon)
    point.Transform(transform)
    print("NPOLAR  (lon, lat)", lon, lat, point.GetX(), point.GetY())
    transform = point = None 
    

    lon = -100.0
    lat = -80.0
    transform = osr.CoordinateTransformation(source, dest_spolar)
    point = ogr.Geometry(ogr.wkbPoint)
    point.AddPoint(lat, lon)
    point.Transform(transform)
    print("SPOLAR  (lon, lat)", lon, lat, point.GetX(), point.GetY())
    transform = point = None 
