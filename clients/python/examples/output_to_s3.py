import os
import configparser
from sliderule import sliderule, icesat2

# Read AWS Credentials #
home_directory = os.path.expanduser( '~' )
aws_credential_file = os.path.join(home_directory, '.aws', 'credentials')
config = configparser.RawConfigParser()
config.read(aws_credential_file)

# Get AWS Credentials #
ACCESS_KEY_ID = config.get('default', 'aws_access_key_id')
SECRET_ACCESS_KEY_ID = config.get('default', 'aws_secret_access_key')
SESSION_TOKEN = config.get('default', 'aws_session_token')

# Initialize SlideRule Client #
sliderule.init("slideruleearth.io", verbose=True)

# Set Parameters #
parms = {
    "output": {
        "path": "s3://sliderule/data/test/testfile.parquet",
        "format": "parquet",
        "open_on_complete": False,
        "region": "us-west-2",
        "credentials": {
            "aws_access_key_id": ACCESS_KEY_ID,
            "aws_secret_access_key": SECRET_ACCESS_KEY_ID,
            "aws_session_token": SESSION_TOKEN
        }
    }
}

# Run Processing Request #
gdf = icesat2.atl06p(parms, resources=['ATL03_20181017222812_02950102_006_02.h5'])
