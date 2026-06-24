from tool import CallableTool

#
# gedi/l1b/subset
#
class L1BSubset(CallableTool):

    def __init__(self):
        super().__init__(
            name="gedi/l1b/subset",
            description="Subset GEDI L1B waveform data by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly", "t0", "t1"]
            }
        )

    def call(self, arguments):
        return None

#
# gedi/l2a/subset
#
class L2ASubset(CallableTool):

    def __init__(self):
        super().__init__(
            name="gedi/l2a/subset",
            description="Subset GEDI L2A elevation data by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly", "t0", "t1"]
            }
        )

    def call(self, arguments):
        return None

#
# gedi/l4a/subset
#
class L4ASubset(CallableTool):

    def __init__(self):
        super().__init__(
            name="gedi/l4a/subset",
            description="Subset GEDI L4A vegetation density data by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly", "t0", "t1"]
            }
        )

    def call(self, arguments):
        return None
