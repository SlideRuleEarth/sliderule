from tool import CallableTool

#
# ams/list_collections
#
class ListCollections(CallableTool):

    def __init__(self):
        super().__init__(
            name="ams/list_collections",
            description="Provides list of available earth data collections in the asset metadata repository",
            schema={
                "type": "object",
                "properties": {},
                "required": []
            }
        )

    def call(self, arguments):
        return None

#
# ams/search
#
class Search(CallableTool):

    def __init__(self):
        super().__init__(
            name="ams/search",
            description="Query collections in the asset metadata repository for list of granules to process",
            schema={
                "type": "object",
                "properties": {},
                "required": []
            }
        )

    def call(self, arguments):
        return None

#
# ams/describe
#
class Describe(CallableTool):

    def __init__(self):
        super().__init__(
            name="ams/describe",
            description="Provide internal layout of collection granules (organization of groupings and variables)",
            schema={
                "type": "object",
                "properties": {},
                "required": []
            }
        )

    def call(self, arguments):
        return None
