import os
import boto3
import uuid
import json
import time
import ctypes
import sliderule
import pyarrow.parquet as pq
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

    @staticmethod
    def datasetUri(mcp_id):
        return f"sliderule://mcp/datasets/{mcp_id}"

    @staticmethod
    def jobUri(mcp_id):
        return f"sliderule://mcp/jobs/{mcp_id}"

    @staticmethod
    def jobKey(mcp_id):
        return f"mcp/job-{mcp_id}"

    @staticmethod
    def jobResource(contents, receipt=None, result=None):
        mcp_id = uuid.uuid4()
        s3.put_object(
            Bucket=PROJECT_PUBLIC_BUCKET,
            Key=Tool.jobKey(mcp_id),
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
                "uri": Tool.jobUri(mcp_id)
            }
        }

    @staticmethod
    def jobStatus(mcp_id):
        obj = s3.get_object(
            Bucket=PROJECT_PUBLIC_BUCKET,
            Key=Tool.jobKey(mcp_id)
        )
        body = obj["Body"].read().decode("utf-8")
        contents = json.loads(body)
        bucket, key = sliderule.splits3uri(contents["receipt"])
        now = time.time()
        try:
            # check for receipt
            s3.head_object(
                Bucket=bucket,
                Key=key
            )
        except ClientError:
            # check time
            if now > (contents["time"] + MAX_JOB_TIME_SECONDS):
                return {
                    "status": "timeout",
                    "created": contents["time"],
                    "updated": now,
                    "dataset": None,
                    "error": f"Processing exceeded maximum time allowed: {MAX_JOB_TIME_SECONDS}"
                }
            else: # receipt does not exist, but timeout has not yet occurred... assume still running
                return {
                    "status": "running",
                    "created": contents["time"],
                    "updated": now,
                    "dataset": None,
                    "error": None
                }
        try:
            # check for result
            bucket, key = sliderule.splits3uri(contents["result"])
            s3.head_object(
                Bucket=bucket,
                Key=key
            )
        except ClientError: # result is missing, failed execution
            return {
                "status": "failed",
                "created": contents["time"],
                "updated": now,
                "dataset": None,
                "error": f"Missing result: {contents["result"]}"
            }
        # success
        return {
            "status": "complete",
            "created": contents["time"],
            "updated": now,
            "dataset": Tool.datasetUri(mcp_id),
            "error": None
        }

    @staticmethod
    def datasetStatus(mcp_id):
        obj = s3.get_object(
            Bucket=PROJECT_PUBLIC_BUCKET,
            Key=Tool.jobKey(mcp_id)
        )
        body = obj["Body"].read().decode("utf-8")
        contents = json.loads(body)
        result_bucket, result_key = sliderule.splits3uri(contents["result"])
        result_head = s3.head_object(Bucket=result_bucket, Key=result_key)
        metadata = pq.read_metadata(contents["result"])
        arrow_schema = metadata.schema.to_arrow_schema()
        openapi_str = ctypes.create_string_buffer(metadata.metadata[b'openapi']).value.decode('ascii')
        openapi_schema = json.loads(openapi_str)
        return {
            "job": Tool.jobKey(mcp_id),
            "created": contents["time"],
            "request": contents["contents"],
            "result": contents["result"],
            "format": "application/x-parquet",
            "size": result_head["ContentLength"],
            "num_rows": metadata.num_rows,
            "columns": {field.name: str(field.type) for field in arrow_schema},
            "openapi": openapi_schema,
            "operations": [
                "data/query",
                "data/export"
            ]
        }
