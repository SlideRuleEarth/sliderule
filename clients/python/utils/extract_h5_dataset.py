#
# Uses the icesat2.h5 api to read a dataset from an H5 file and write the contents to a file
#

import sys
import logging
from sliderule import sliderule
from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)

    # Set URL #
    url = ["127.0.0.1"]
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Set Asset #
    asset = "atlas-local"
    if len(sys.argv) > 2:
        asset = sys.argv[2]

    # Set Dataset #
    dataset = "/gt2l/heights/h_ph"
    if len(sys.argv) > 3:
        dataset = sys.argv[3]

    # Set Resource #
    resource = "ATL03_20181017222812_02950102_003_01.h5"
    if len(sys.argv) > 4:
        resource = sys.argv[4]

    # Bypass service discovery if url supplied
    if len(sys.argv) > 5:
        if sys.argv[5] == "bypass":
            url = [url]

    # Set Subset #
    col         = 0
    startrow    = 0
    numrows     = -1
    if len(sys.argv) > 7:
        col         = int(sys.argv[6]) # 0
        startrow    = int(sys.argv[7]) # 13665185
        numrows     = int(sys.argv[8]) # 89624

    # Configure SlideRule #
    icesat2.init(url, True)

    # Read Dataset #
    datasets = [ {"dataset": dataset, "col": col, "startrow": startrow, "numrows": numrows} ]
    rawdata = icesat2.h5p(datasets, resource, asset)

    # Write Dataset to File #
    filename = dataset[dataset.rfind("/")+1:]
    f = open(filename + ".bin", 'w+b')
    f.write(bytearray(rawdata[dataset]))
    f.close()
