#
# Requests SlideRule to process the provided region of interest
# and write the results to an HDF5 file.  Useful for large regions.
#

import sys
import time
import logging
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

    # Region of Interest #
    region_filename = sys.argv[1]
    region = icesat2.toregion(region_filename)

    # set script defaults
    cfg = {
        "run": 'write',
        "url": 'icesat2sliderule.org',
        "asset": 'nsidc-s3',
        "filename": 'atl06.hdf5',
        "max_resources": 100000
    }

    # set processing parameter defaults
    parms = {
        "poly": region["poly"],
        "raster": region["raster"],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "atl08_class": ["atl08_ground"],
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # get command line parameters
    parse_command_line(sys.argv, cfg)
    parse_command_line(sys.argv, parms)

    if cfg["run"] == 'write':
        # configure SlideRule
        icesat2.init(cfg["url"], False, cfg["max_resources"], loglevel=logging.INFO)

        # create hdf5 writer
        hdf5writer = Hdf5Writer(cfg["filename"], parms)

        # atl06 processing request
        perf_start = time.perf_counter()
        icesat2.atl06p(parms, cfg["asset"], max_workers=cfg["max_workers"], callback=lambda resource, result, index, total : hdf5writer.run(resource, result, index, total))
        print("Completed in {:.3f} seconds of wall-clock time".format(time.perf_counter() - perf_start))

        # close hdf5 file
        hdf5writer.finish()

    elif cfg["run"] == 'read':
        # create hdf5 reader
        hdf5reader = Hdf5Reader(cfg["filename"])
        # display GeoDataFrame
        print(hdf5reader.gdf.describe())
