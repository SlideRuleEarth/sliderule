import sys

lat_deg = sys.argv[1]
lat_min = sys.argv[2]
lat_hem = sys.argv[3]
lon_deg = sys.argv[4]
lon_min = sys.argv[5]
lon_hem = sys.argv[6]

lat = lat_deg + (lat_min / 60)
