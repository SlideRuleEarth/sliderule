# python
import sys
import h5coro

# set resource
resource = "file:///data/ATLAS/ATL06_20200714160647_02950802_003_01.h5"

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    h5file = h5coro.file(resource)
    h_li = h5file.read("/gt1l/land_ice_segments/h_li", 0, 20, 5)
    print(h_li)
