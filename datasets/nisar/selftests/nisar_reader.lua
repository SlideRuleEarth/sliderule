local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({"asf-cloud"})
-- runner.log(core.DEBUG)

local srcfile, dirpath = runner.srcscript()
local geojsonfile = dirpath.."../data/nisar_fake.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f, "failed to open geojson file")
if not f then return end
local contents = f:read("*all")
f:close()

-- Correct values test for different POIs

local numPoints = 1000  -- Set to 1 to use a specific fixed point

local lons, lats, heights = {}, {}, {}

if numPoints == 1 then
    -- Use hardcoded test point
    lons = { -117.9705 }
    lats = {   34.8005 }
    heights = { 0 }
else
    -- Generate lon/lat arrays across the geographic extent
    -- Original extent
    local lonMin, lonMax = -118.4603, -117.4807
    local latMin, latMax =  34.2325,   35.3646

    -- Contract the extent by margin %, to avoid edge cases with many 'nan' values
    local margin = 0.25
    local lonRange = lonMax - lonMin
    local latRange = latMax - latMin

    lonMin = lonMin + margin * lonRange
    lonMax = lonMax - margin * lonRange
    latMin = latMin + margin * latRange
    latMax = latMax - margin * latRange

    -- Generate points
    for i = 1, numPoints do
        local t = (i - 1) / (numPoints - 1)
        table.insert(lons, lonMin + t * (lonMax - lonMin))
        table.insert(lats, latMin + t * (latMax - latMin))
        table.insert(heights, 0)
    end
end

print(string.format("\n--------------------------------\nTest: NISAR L2 GEOFF (%d point%s)\n--------------------------------", numPoints, numPoints == 1 and "" or "s"))


local dem = geo.raster(geo.parms({
    asset = "nisar-L2-geoff",
    algorithm = "NearestNeighbour",
    catalog = contents,
    sort_by_index = true
}))
runner.assert(dem ~= nil, "failed to create nisar-L2 dataset", true)

local batchResults = {}
local starttime = time.latch();
tbl, err = dem:batchsample(lons, lats, heights)

runner.assert(err == 0)
runner.assert(tbl ~= nil)

if err ~= 0 then
    table.insert(batchResults, nil)
else
    batchResults = tbl
end

dtime = time.latch() - starttime
print(string.format("Batch sampling %d points, ExecTime: %f\n", numPoints, dtime))


-- Report Results --
runner.report()


