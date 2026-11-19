import sys
from sliderule import sliderule

sliderule.init("localhost", organization=None, verbose=True)

resources = [granule.strip().replace(".parquet", ".h5") for granule in open("/data/ATL24/atl24r3_bluetopo_granules.txt") if granule.strip()]
print(f"Ready to process {len(resources)} granules")

count = 1
for resource in resources:
    print(f"[{count}/{len(resources)}] Processing {resource}... ")
    parms = {
        "output": {
            "asset": "sliderule-stage",
            "path": f"atl24r3/bluetopo/{resource.replace('.h5', '_samples.parquet')}",
            "format": "geoparquet",
        },
        "samples": {
            "bluetopo": {
                "asset": "bluetopo-bathy",
                "bands": ["Elevation", "Uncertainty"]
            }
        }
    }
    rsps = sliderule.run("atl24x", parms, resources=[resource])
    print(f"[{count}/{len(resources)}] Completed {rsps} ")
    count += 1