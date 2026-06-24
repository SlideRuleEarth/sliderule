import boto3

# ###############################
# Cached Globals
# ###############################

s3 = boto3.client("s3")

# ###############################
# Base Tool Class
# ###############################

class CallableTool:

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
