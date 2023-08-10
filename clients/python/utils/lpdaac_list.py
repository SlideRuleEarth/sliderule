from sliderule import sliderule, earthdata
import sys

earthdata.set_max_resources(2000)

geojson = sys.argv[1]
region = sliderule.toregion(geojson)
granules = earthdata.cmr(short_name="GEDI02_A", polygon=region["poly"])

for granule in granules:
    print(f'{__file__[:__file__.rfind("/")]}/lpdaac_transfer.sh {granule}')
