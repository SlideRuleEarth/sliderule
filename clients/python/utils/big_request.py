#
# Requests SlideRule to process the provided region of interest
# and write the results to an HDF5 file.  Useful for large regions.
#

import sys
import time
import logging
import geopandas
import folium
import matplotlib.pyplot as plt
from sliderule import icesat2
from sliderule.stream2disk import Hdf5Writer, Hdf5Reader

def parse_command_line(args, cfg):
    i = 1
    while i < len(args):
        for entry in cfg:
            if args[i] == '--'+entry:
                if type(cfg[entry]) is str:
                    cfg[entry] = args[i + 1]
                elif type(cfg[entry]) is list:
                    l = []
                    while (i + 1) < len(args) and args[i + 1].isnumeric():
                        l.append(int(args[i + 1]))
                        i += 1
                    cfg[entry] = l
                elif type(cfg[entry]) is int:
                    cfg[entry] = int(args[i + 1])
                i += 1
        i += 1
    return cfg

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # positional command line parameters
    region_filename = sys.argv[1]

    # set script defaults
    cfg = {
        "run": 'write', # run, plot, list
        "url": 'icesat2sliderule.org',
        "asset": 'nsidc-s3',
        "filename": 'atl06.hdf5',
        "tolerance": 0.0,
        "cellsize": 0.01,
        "n_clusters": 1,
        "max_resources": 100000
    }

    # parse configuration parameters
    parse_command_line(sys.argv, cfg)

    # build region
    region = icesat2.toregion(region_filename, tolerance=cfg["tolerance"], cellsize=cfg["cellsize"], n_clusters=cfg["n_clusters"])

    # set processing parameter defaults
    parms = {
        "poly": region["poly"],
        "raster": region["raster"],
        "clusters": region["clusters"],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "atl08_class": ["atl08_ground"],
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # parse processing parametera
    parse_command_line(sys.argv, parms)

    # run script
    if cfg["run"] == 'write':

        # configure SlideRule
        icesat2.init(cfg["url"], False, cfg["max_resources"], loglevel=logging.INFO)

        # create hdf5 writer
        hdf5writer = Hdf5Writer(cfg["filename"], parms)

        # atl06 processing request
        perf_start = time.perf_counter()
        icesat2.atl06p(parms, cfg["asset"], callback=lambda resource, result, index, total : hdf5writer.run(resource, result, index, total))
        print("Completed in {:.3f} seconds of wall-clock time".format(time.perf_counter() - perf_start))

        # close hdf5 file
        hdf5writer.finish()

    elif cfg["run"] == 'read':

        # create hdf5 reader
        hdf5reader = Hdf5Reader(cfg["filename"])

        # display GeoDataFrame
        print(hdf5reader.gdf.describe())

    elif cfg["run"] == 'plot':

        # Create Plot
        fig = plt.figure(num=None, figsize=(24, 12))

        # Plot Encasing Polygon
        ax1 = plt.subplot(121)
        lons = [p["lon"] for p in region["poly"]]
        lats = [p["lat"] for p in region["poly"]]
        ax1.plot(lons, lats, linewidth=1.5, color='r', zorder=2)

        # Plot Cluster Polygons
        ax2 = plt.subplot(122)
        for cluster in region["clusters"]:
            lons = [p["lon"] for p in cluster]
            lats = [p["lat"] for p in cluster]
            ax2.plot(lons, lats, linewidth=1.5, color='r', zorder=2)

        # Show Plot
        plt.show()

    elif cfg["run"] == 'display':

        # read into pandas
        pregion = geopandas.read_file(region_filename)

        # build folium map
        m = folium.Map(location=[region["poly"][0]["lat"], region["poly"][0]["lon"]], zoom_start=5)
        folium.Choropleth(pregion, line_color='blue', line_weight=1.5).add_to(m)

        # draw containing polygon
        lons = [p["lon"] for p in region["poly"]]
        lats = [p["lat"] for p in region["poly"]]
        points = [(lat, lon) for lat,lon in zip(lats,lons)]
        folium.PolyLine(points, color="green", weight=2.0, opacity=1).add_to(m)

        # draw polygon clusters
        for cluster in region["clusters"]:
            lons = [p["lon"] for p in cluster]
            lats = [p["lat"] for p in cluster]
            points = [(lat, lon) for lat,lon in zip(lats,lons)]
            folium.PolyLine(points, color="red", weight=2.5, opacity=1).add_to(m)

        # display raster
        html_filename = "map.html"
        m.save(html_filename)

    elif cfg["run"] == 'list':

        # get polygon(s)
        polygon = region["poly"]
        if len(region["clusters"]) > 0:
            polygon = region["clusters"]

        # query CMR
        resources = icesat2.cmr(polygon=polygon)

        # display resources
        for resource in resources:
            print(resource)
        print("Total: ", len(resources))