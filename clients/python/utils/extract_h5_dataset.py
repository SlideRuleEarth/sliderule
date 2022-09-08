#
# Uses the icesat2.h5p api to read a dataset from an H5 file and write the contents to a file
#

import sys
import logging
from sliderule import icesat2
from utils import parse_command_line

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # set script defaults
    cfg = {
        "url":          'localhost',
        "organization": None,
        "asset":        'atlas-local',
        "dataset":      '/gt2l/heights/h_ph',
        "resource":     'ATL03_20181017222812_02950102_005_01.h5',
        "col":          0,
        "startrow":     0,
        "numrows":      -1
    }

    # parse configuration parameters
    parse_command_line(sys.argv, cfg)

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)

    # Configure SlideRule #
    icesat2.init(cfg["url"], True, organization=cfg["organization"])

    # Read Dataset #
    datasets = [ {"dataset": cfg["dataset"], "col": cfg["col"], "startrow": cfg["startrow"], "numrows": cfg["numrows"]} ]
    rawdata = icesat2.h5p(datasets, cfg["resource"], cfg["asset"])
    print(rawdata)

    # Write Dataset to File #
    filename = cfg["dataset"][cfg["dataset"].rfind("/")+1:]
    f = open(filename + ".bin", 'w+b')
    f.write(bytearray(rawdata[cfg["dataset"]]))
    f.close()
