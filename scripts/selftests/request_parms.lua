local runner = require("test_executive")
local console = require("console")
local prettyprint = require("prettyprint")
local json = require("json")

-- Setup

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

-- (1) Defaults

print('---------- Defaults ----------')

local parms = core.parms()
local ptable = parms:export()

runner.check(ptable["rqst_timeout"] == core.RQST_TIMEOUT)
runner.check(ptable["node_timeout"] == core.NODE_TIMEOUT)
runner.check(ptable["read_timeout"] == core.READ_TIMEOUT)
runner.check(ptable["cluster_size_hint"] == 0)
runner.check(ptable["points_in_polygon"] == 0)
runner.check(ptable["region_mask"]["rows"] == 0)
runner.check(ptable["region_mask"]["cols"] == 0)

prettyprint.display(ptable)

-- (2) Timeouts

print('---------- Timeouts ----------')

parms = core.parms({timeout=400})
ptable = parms:export()

runner.check(ptable["rqst_timeout"] == 400)
runner.check(ptable["node_timeout"] == 400)
runner.check(ptable["read_timeout"] == 400)

parms = core.parms({rqst_timeout=400})
ptable = parms:export()

runner.check(ptable["rqst_timeout"] == 400)
runner.check(ptable["node_timeout"] == core.NODE_TIMEOUT)
runner.check(ptable["read_timeout"] == core.READ_TIMEOUT)

-- (3) Cluster Size Hint

print('---------- Cluster Size Hint ----------')

parms = core.parms({cluster_size_hint=9})
ptable = parms:export()

runner.check(ptable["cluster_size_hint"] == 9)

-- (4) Projected Polygon

print('---------- Projected Polygon ----------')

local grandmesa = { {lon=-108.3435200747503, lat=38.89102961045247},
                    {lon=-107.7677425431139, lat=38.90611184543033},
                    {lon=-107.7818591266989, lat=39.26613714985466},
                    {lon=-108.3605610678553, lat=39.25086131372244},
                    {lon=-108.3435200747503, lat=38.89102961045247} }

parms = core.parms({poly=grandmesa})
ptable = parms:export()

runner.check(math.abs(ptable["poly"][1]["lon"] - grandmesa[1]["lon"]) < 0.001)
runner.check(math.abs(ptable["poly"][1]["lat"] - grandmesa[1]["lat"]) < 0.001)

runner.check(parms:polygon(-108.0, 39.0))
runner.check(not parms:polygon(-110, 40.0))

-- (5) Region Mask

print('---------- Region Mask ----------')

local dicksonfjord = '{"type":"FeatureCollection","features":[{"type":"Feature","properties":{},"geometry":{"coordinates":[[[-27.324285913258706,72.91303911622592],[-27.678594995289217,72.80131917669064],[-27.313299585132967,72.60615538614448],[-26.434393335133166,72.69461098330294],[-26.173468042164387,72.83782721550836],[-26.448126245289757,72.942067469947],[-27.324285913258706,72.91303911622592]]],"type":"Polygon"}}]}'

parms = core.parms({region_mask={geojson=dicksonfjord, cellsize=0.001}})
ptable = parms:export()

runner.check(ptable["region_mask"]["rows"] == 335)
runner.check(ptable["region_mask"]["cols"] == 1505)
runner.check(math.abs(ptable["region_mask"]["lonmin"] - -27.678594995289) < 0.001)
runner.check(math.abs(ptable["region_mask"]["latmin"] - 72.607067469947) < 0.001)
runner.check(math.abs(ptable["region_mask"]["lonmax"] - -26.173594995289) < 0.001)
runner.check(math.abs(ptable["region_mask"]["latmax"] - 72.942067469947) < 0.001)

runner.check(parms:mask(-27.0, 72.8))
runner.check(not parms:mask(-26.0, 73.0))

prettyprint.display(ptable)

-- Clean Up --

-- Report Results --

runner.report()
