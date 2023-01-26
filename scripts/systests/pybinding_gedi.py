# python

import srpybin
import requests

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

# python function to get s3 credentials
def get_s3credentials(daac: str):
    earthadata_s3 = f"https://data.{daac}.earthdata.nasa.gov/s3credentials"
    r = requests.get(earthadata_s3)
    r.raise_for_status()
    return r.json()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    s3credentials = get_s3credentials("ornldaac")
    srcredentials = srpybin.credentials("ornldaac")
    srcredentials.provide({"accessKeyId":       s3credentials['accessKeyId'],
                          "secretAccessKey":    s3credentials['secretAccessKey'],
                          "sessionToken":       s3credentials['sessionToken'],
                          "expiration":         s3credentials["expiration"]})
    c = srcredentials.retrieve()
    print(c)

    resource = "GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5"
    format = "s3"
    path = "ornl-cumulus-prod-protected/gedi/GEDI_L4A_AGB_Density_V2_1/data"
    region = "us-west-2"
    endpoint = "https://s3.us-west-2.amazonaws.com"
    datasets = [["/BEAM0000/lat_lowestmode", 0, 0, -1],
                ["/BEAM0000/lon_lowestmode", 0, 0, -1],
                ["/BEAM0000/agbd", 0, 0, -1]]

    h5file = srpybin.h5coro(resource, format, path, region, endpoint)
    data = h5file.readp(datasets)

    print(data)
