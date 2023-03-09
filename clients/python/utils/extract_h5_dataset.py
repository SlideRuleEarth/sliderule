#
# Uses the icesat2.h5p api to read a dataset from an H5 file and write the contents to a file
#

import sys
from sliderule import icesat2
from utils import parse_command_line, initialize_client

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # set script defaults
    local = {
        "dataset":      '/gt2l/heights/h_ph',
        "col":          0,
        "startrow":     0,
        "numrows":      -1
    }

    # Initialize Client
    parms, cfg = initialize_client(sys.argv)

    # parse configuration parameters
    parse_command_line(sys.argv, local)

    # Read Dataset #
    datasets = [ {"dataset": local["dataset"], "col": local["col"], "startrow": local["startrow"], "numrows": local["numrows"]} ]
    rawdata = icesat2.h5p(datasets, cfg["resource"], cfg["asset"])
    print(rawdata)

    # Write Dataset to File #
    filename = local["dataset"][local["dataset"].rfind("/")+1:]
    f = open(filename + ".bin", 'w+b')
    f.write(bytearray(rawdata[local["dataset"]]))
    f.close()
