from tool import Tool

#
# data/describe
#
class DataDescribe(Tool):

    def __init__(self):
        super().__init__(
            name="data/describe",
            description="Provides metadata, schema, and downloadable url of dataset resource",
            schema={
                "type": "object",
                "properties": {
                    "resource": {
                        "type": "string",
                        "description": "dataset resource uri in the form sliderule://mcp/datasets/{uuid}"
                    }
                },
                "required": ["resource"]
            }
        )

    def call(self, arguments):
        return None

#
# data/query
#
class DataQuery(Tool):

    def __init__(self):
        super().__init__(
            name="data/describe",
            description="Executes SQL on dataset resources and returns results or handle to another resource",
            schema={
                "type": "object",
                "properties": {
                    "resource": {
                        "type": "string",
                        "description": "dataset resource uri in the form sliderule://mcp/datasets/{uuid}"
                    },
                    "sql": {
                        "type": "string",
                        "description": "SQL to execute against dataset"
                    }
                },
                "required": ["resource"]
            }
        )

    def call(self, arguments):
        return None