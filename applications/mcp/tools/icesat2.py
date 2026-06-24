from tool import CallableTool

#
# icesat2/atl06/subset
#
class atl06_subset(CallableTool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl06/subset",
            description="Subset ICESat-2 ATL06 land ice height data by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly", "t0", "t1"]
            }
        )

    def call(self, arguments):
        return None