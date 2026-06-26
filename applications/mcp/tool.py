import boto3

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
