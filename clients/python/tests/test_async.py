"""Tests for async requests"""

import time
import boto3
from botocore.exceptions import ClientError
import geopandas as gpd
from sliderule import sliderule

s3 = boto3.client("s3", region_name="us-west-2")

def object_exists(bucket, key):
    try:
        s3.head_object(Bucket=bucket, Key=key)
        return True
    except ClientError as e:
        if e.response['Error']['Code'] == '404':
            return False
        raise  # re-raise unexpected errors

class TestAsync:
    def test_atl03x(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "output": {
                "asset": "sliderule-stage"
            },
            "resources": [
                "ATL03_20181019065445_03150111_006_02.h5"
            ],
            "poly": [
                { "lat": -80.90, "lon": -70.00 },
                { "lat": -81.00, "lon": -70.00 },
                { "lat": -81.00, "lon": -65.00 },
                { "lat": -80.90, "lon": -65.00 },
                { "lat": -80.90, "lon": -70.00 }
            ]
        }
        rsps = sliderule.source("atl03x.async", {"parms": parms})
        assert init
        obj_path = rsps["receipt"].split("s3://")[-1]
        obj_elem = obj_path.split("/")
        receipt_bucket = obj_elem[0]
        receipt_key = '/'.join(obj_elem[1:])
        time_remaining = 10
        while not object_exists(receipt_bucket, receipt_key):
            if time_remaining <= 0:
                assert False, "timed out waiting for async processing request to finish"
            print(f"Waiting for {rsps["receipt"]} ... waiting {time_remaining} seconds")
            time.sleep(1)
        receipt_obj = s3.get_object(Bucket=receipt_bucket, Key=receipt_key)
        receipt_contents = receipt_obj["Body"].read().decode("utf-8")
        assert receipt_contents.count("\n") == 6
        output = rsps["parameters"]["output"]["path"]
        gdf = gpd.read_parquet(output)
        assert len(gdf) == 195853
