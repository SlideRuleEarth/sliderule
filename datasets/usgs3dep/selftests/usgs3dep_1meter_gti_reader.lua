local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local geojsonfile = dirpath.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Self Test --

local  sigma = 1.0e-4
local  lon =    -108.1
local  lat =      39.1
local  height =    0.0

print(string.format("\n-------------------------------------------------\nusgs3dep 1meter GTI DEM sample\n-------------------------------------------------"))


local demType = "usgs3dep-1meter-gti-dem"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents, sort_by_index = true }))
local tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1
end


-- Report Results --

runner.report()
