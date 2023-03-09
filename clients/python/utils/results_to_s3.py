import os
import sys
import configparser
import sliderule
from sliderule import icesat2
from utils import initialize_client

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
parms, cfg = initialize_client(sys.argv)

# Set Output Parameters #
parms["output"] = {
    "path": "s3://sliderule/config/testfile.parquet",
    "format": "parquet",
    "open_on_complete": False,
    "region": "us-west-2",
    "credentials": {
        "aws_access_key_id": ACCESS_KEY_ID,
        "aws_secret_access_key": SECRET_ACCESS_KEY_ID,
        "aws_session_token": SESSION_TOKEN
    }
}

# Run Processing Request #
gdf = icesat2.atl06p(parms, asset=cfg["asset"], resources=[cfg["resource"]])
