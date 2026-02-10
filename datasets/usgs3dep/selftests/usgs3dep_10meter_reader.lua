local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local  sigma = 1.0e-4

local lons = { -108.1}
local lats = {   39.1}
local heights = { 0 }

-- Self Test --

runner.unittest("3DEP 10m DEM Sample (serial)", function()

    local expResults = {{2638.235986840923, 1354129161.0, '/vsis3/prd-tnm/StagedProducts/Elevation/13/TIFF/USGS_Seamless_DEM_13.vrt'}}

    local demType = "usgs3dep-10meter-dem"
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
    local tbl, err = dem:sample(lons[1], lats[1], heights[1])
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    local sampleCnt = 0
    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        local time = v["time"]
        print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
        sampleCnt = sampleCnt + 1

        if tostring(el) ~= "nan" then
            runner.assert(math.abs(el - expResults[i][1]) < sigma)
        end
        runner.assert(time == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
    runner.assert(sampleCnt == #expResults)

end)

runner.unittest("3DEP 10m DEM Sample (batch)", function()

    local numPoints = 1000
    local lonMin, lonMax = 100, 180
    local latMin, latMax =  25,  50

    -- Generate points
    local lons = {}
    local lats = {}
    local heights = {}
    for i = 1, numPoints do
        local t = (i - 1) / (numPoints - 1)
        table.insert(lons, lonMin + t * (lonMax - lonMin))
        table.insert(lats, latMin + t * (latMax - latMin))
        table.insert(heights, 0)
    end

    local tbl, err = dem:batchsample(lons, lats, heights)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)
    runner.assert(#tbl == numPoints, string.format("Expected %d samples, got %d", numPoints, #tbl))

end)

-- Report Results --

runner.report()
