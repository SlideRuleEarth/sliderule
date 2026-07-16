from tool import Tool
from sliderule import sliderule

#
# Globals
#
ALONG_TRACK_SEGMENT_PROPERTIES = {
    "res": {
        "type": "number",
        "minimum": 1,
        "maximum": 500,
        "description": "Along-track spacing between successive elevation estimates, in meters."
    },
    "len": {
        "type": "number",
        "minimum": 1,
        "maximum": 100,
        "description": "Length of the photon segment used to compute each elevation estimate, in meters."
    }
}
GRANULE_PROPERTIES = {
    "resources": {
        "type": "array",
        "items": {
            "type": "string"
        },
        "description": "List of granule names"
    }
}

#
# icesat2/atl03/subset
#
class Atl03Subset(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl03/subset",
            description="Subset ICESat-2 ATL03 photon cloud data by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES | GRANULE_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        rsps = sliderule.source("atl03x.async", {"parms": arguments | {
            "poly": sliderule.toregion(arguments["poly"])["poly"],
            "output": {"asset": "sliderule-stage", "with_openapi": True}
        }}, rethrow=True)
        return self.jobResource(rsps, receipt=rsps["receipt"], result=rsps["parameters"]["output"]["path"])

#
# icesat2/atl03/surface_fit
#
class Atl03SurfaceFit(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl03/surface_fit",
            description="Fit a surface to the ICESat-2 ATL03 photon cloud and return along track elevations",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES | GRANULE_PROPERTIES | ALONG_TRACK_SEGMENT_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        return None

#
# icesat2/atl03/density_metrics
#
class Atl03DensityMetrics(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl03/density_metrics",
            description="Calculate density metrics of ICESat-2 ATL03 photon cloud used for characterizing the vertical profile of the vegetation. This tool is preferred when custom along track segmentation is needed; otherwise use icesat2/atl08/subset.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES | ALONG_TRACK_SEGMENT_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        return None

#
# icesat2/atl06/subset
#
class Atl06Subset(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl06/subset",
            description="Subset ICESat-2 ATL06 land ice height data by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        return None

#
# icesat2/atl08/subset
#
class Atl08Subset(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl08/subset",
            description="Subset ICESat-2 ATL08 vegetation density metrics by spatial and temporal filters. This tool is preferred when custom along-track segmentation is not needed and the default 100 meter posting is acceptable; otherwise use icesat2/atl03/density_metrics.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        return None

#
# icesat2/atl13/subset
#
class Atl13Subset(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl13/subset",
            description="Subset ICESat-2 ATL13 inland water metrics by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        return None

#
# icesat2/atl24/subset
#
class Atl24Subset(Tool):

    def __init__(self):
        super().__init__(
            name="icesat2/atl24/subset",
            description="Subset ICESat-2 ATL24 near-shore bathymetry by spatial and temporal filters.",
            schema={
                "type": "object",
                "properties": self.GEOSPATIAL_TEMPORAL_PROPERTIES,
                "required": ["poly"]
            }
        )

    def call(self, arguments):
        return None