#
# Uses the icesat2.h5 api to read a dataset from an H5 file and write the contents to a file
#

import sys
import logging
from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url             = "127.0.0.1"
    asset           = "atlas-local"
    organization    = None
    dataset         = "/gt2l/heights/h_ph"
    resource        = "ATL03_20181017222812_02950102_003_01.h5"
    col             = 0
    startrow        = 0
    numrows         = -1

    # Set URL #
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Set Asset #
    if len(sys.argv) > 2:
        asset = sys.argv[2]

    # Set Organization
    if len(sys.argv) > 3:
        organization = sys.argv[3]

    # Set Dataset #
    if len(sys.argv) > 4:
        dataset = sys.argv[4]

    # Set Resource #
    if len(sys.argv) > 5:
        resource = sys.argv[5]

    # Set Subset #
    if len(sys.argv) > 8:
        col         = int(sys.argv[6]) # 0
        startrow    = int(sys.argv[7]) # 13665185
        numrows     = int(sys.argv[8]) # 89624

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)

    # Configure SlideRule #
    icesat2.init(url, True, organization=organization)

    # Read Dataset #
    datasets = [ {"dataset": dataset, "col": col, "startrow": startrow, "numrows": numrows} ]
    rawdata = icesat2.h5p(datasets, resource, asset)
    print(rawdata)

    # Write Dataset to File #
    filename = dataset[dataset.rfind("/")+1:]
    f = open(filename + ".bin", 'w+b')
    f.write(bytearray(rawdata[dataset]))
    f.close()
