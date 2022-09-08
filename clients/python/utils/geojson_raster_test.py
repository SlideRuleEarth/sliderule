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


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = "testsliderule.org"
    asset = "nsidc-s3"
    resource = "ATL03_20181017222812_02950102_005_01.h5"

    # Configure SlideRule #
    icesat2.init(url, True)

    tracks     = [0, 0, 0]
    photons    = [0, 0, 0]
    cycles     = [0, 0, 0]
    exectime   = [0, 0, 0]

    test_files = ["examples/grandmesa.geojson", "examples/grandmesa.shp"]

    for i, testfile in enumerate(test_files):
        print("Testing with:", testfile)

        # Region of Interest #
        region = icesat2.toregion(testfile)

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
        gdf = icesat2.atl03s(parms, resource, asset)

        # Latch Stop Time
        perf_stop = time.perf_counter()
        perf_duration = perf_stop - perf_start

        # Build DataFrame of SlideRule Responses
        tracks[i]  = gdf["rgt"].unique()
        photons[i] = len(gdf)
        cycles[i]  = gdf["cycle"].unique()
        exectime[i]= perf_duration

    # Display Statistics
    print("\n")
    for i, testfile in enumerate(test_files):
        # print("Tracks: {0:4d}, Photns: {1:8d}, Cycles: {2:4d}, ExecTime: {3:.3f}, {4:s}".
        print("Tracks: {0}, Photns: {1}, Cycles: {2}, ExecTime: {3:.3f}, {4}".
              format(tracks[i], photons[i], cycles[i], exectime[i], testfile ))