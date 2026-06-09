"""Tests for h5 endpoint."""

import pytest
import boto3
from sliderule import sliderule

class TestS3:
    def test_v4_sha256_checksum(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "output": {
                "asset": "sliderule-stage",
                "with_checksum": True,
                "open_on_complete": False
            }
        }
        aoi = [
            { "lat": -80.75, "lon": -70.00 },
            { "lat": -81.00, "lon": -70.00 },
            { "lat": -81.00, "lon": -65.00 },
            { "lat": -80.75, "lon": -65.00 },
            { "lat": -80.75, "lon": -70.00 }
        ]
        resources = [
            "ATL03_20181019065445_03150111_006_02.h5"
        ]
        remote_obj_path = sliderule.run("atl03x", parms, aoi, resources)
        path_without_scheme = remote_obj_path[len("s3://"):]
        bucket, key = path_without_scheme.split("/", 1)
        s3 = boto3.client("s3")
        response = s3.head_object(Bucket=bucket, Key=key, ChecksumMode="ENABLED")
        sha256_checksum = response.get("ChecksumSHA256")
        assert init
        assert remote_obj_path.startswith("s3://")
        assert sha256_checksum is not None
        assert len(sha256_checksum) == 44
