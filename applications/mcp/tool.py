import os
import boto3
import uuid
import json
import time
import geopandas as gpd
from botocore.exceptions import ClientError

# ###############################
# Environment
# ###############################

PROJECT_PUBLIC_BUCKET = os.environ.get("PROJECT_PUBLIC_BUCKET")
MAX_JOB_TIME_SECONDS = 600 # 10 minutes

# ###############################
# Cached Globals
# ###############################

s3 = boto3.client("s3")

# ###############################
# Base Content Class
# ###############################

class Content:

    def __init__(self, msg_type, content):
        self.type = msg_type
        self.content = content

    @property
    def definition(self):
        if self.type == "text":
            return {
                "type": "text",
                "text": self.content
            }
        elif self.type == "resource":
            return {
                "type": "text",
                "resource": {
                    "uri": self.content
                }
            }
        else:
            raise RuntimeError(f"invalid content type: {self.type}")


# ###############################
# Base Tool Class
# ###############################

class Tool:

    GEOSPATIAL_TEMPORAL_PROPERTIES = {
        "poly": {
            "type": "string",
            "description": "GeoJSON polygon for spatial filtering"
        },
        "t0": {
            "type": "string",
            "description": "ISO 8601 start time"
        },
        "t1": {
            "type": "string",
            "description": "ISO 8601 end time"
        }
    }

    def __init__(self, name, description, schema):
        self.name = name
        self.description = description
        self.schema = schema

    @property
    def definition(self):
        return {
            "name": self.name,
            "description": self.description,
            "inputSchema": self.schema
        }

    def call(self, arguments):
        raise NotImplementedError()

    def jobResource(contents, receipt=None, result=None):
        job_id = uuid.uuid4()
        s3.put_object(
            Bucket=PROJECT_PUBLIC_BUCKET,
            Key=f"mcp/job-{job_id}",
            Body=json.dumps({
                "time": time.time(),
                "receipt": receipt,
                "result": result,
                "contents": contents
            })
        )
        return {
            "type": "resource",
            "resource": {
                "uri": f"sliderule://job/{job_id}"
            }
        }

    def jobStatus(job_id):
        obj = s3.get_object(
            Bucket=PROJECT_PUBLIC_BUCKET,
            Key=f"mcp/job-{job_id}"
        )
        body = obj["Body"].read().decode("utf-8")
        contents = json.loads(body)
        now = time.time()
        try:
            # check for receipt
            s3.head_object(
                Bucket=PROJECT_PUBLIC_BUCKET,
                Key=contents["receipt"]
            )
        except ClientError:
            # check time
            if now > (contents["time"] + MAX_JOB_TIME_SECONDS):
                return {
                    "uri": f"sliderule://job/{job_id}",
                    "mimeType": "application/json",
                    "text": json.dumps({
                        "status": "timeout",
                        "created": contents["time"],
                        "updated": now,
                        "dataset": None,
                        "error": f"Processing exceeded maximum time allowed: {MAX_JOB_TIME_SECONDS}"
                    })
                }
            else: # receipt does not exist, but timeout has not yet occurred... assume still running
                return {
                    "uri": f"sliderule://job/{job_id}",
                    "mimeType": "application/json",
                    "text": json.dumps({
                        "status": "running",
                        "created": contents["time"],
                        "updated": now,
                        "dataset": None,
                        "error": None
                    })
                }
        try:
            # check for result
            s3.head_object(
                Bucket=PROJECT_PUBLIC_BUCKET,
                Key=contents["result"]
            )
        except ClientError: # result is missing, failed execution
            return {
                "uri": f"sliderule://job/{job_id}",
                "mimeType": "application/json",
                "text": json.dumps({
                    "status": "failed",
                    "created": contents["time"],
                    "updated": now,
                    "dataset": None,
                    "error": f"Missing result: {contents["result"]}"
                })
            }
        # success
        return {
            "uri": f"sliderule://job/{job_id}",
            "mimeType": "application/json",
            "text": json.dumps({
                "status": "complete",
                "created": contents["time"],
                "updated": now,
                "dataset": f"sliderule://dataset/{job_id}",
                "error": None
            })
        }

    def datasetStatus(job_id):
        obj = s3.get_object(
            Bucket=PROJECT_PUBLIC_BUCKET,
            Key=f"mcp/job-{job_id}"
        )
        body = obj["Body"].read().decode("utf-8")
        contents = json.loads(body)
        gdf = gpd.read_parquet(contents["result"])

        return {
            "uri": f"sliderule://dataset/{job_id}",
            "mimeType": "application/json",
            "text": json.dumps({
                "created": contents["time"],
                "format": "application/x-parquet",
                "rows": len(gdf),
                "schema": {col: str(dtype) for col, dtype in gdf.dtypes.items()}
            })
        }