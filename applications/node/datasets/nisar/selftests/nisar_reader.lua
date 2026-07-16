local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({"asf-cloud"})

local numPoints = 1000  -- Set to 1 to use a specific fixed point

-- Self Test --

runner.unittest(string.format("NISAR (%d point%s)", numPoints, numPoints == 1 and "" or "s"), function()

    local geojsonfile = dirpath.."../data/nisar_fake.geojson"
    local f = io.open(geojsonfile, "r")
    assert(f, "failed to open geojson file")
    local contents = f:read("*all")
    f:close()

    -- Correct values test for different POIs
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

    local dem = geo.raster(geo.parms({
        asset = "nisar-L2-geoff",
        algorithm = "NearestNeighbour",
        catalog = contents,
        sort_by_index = true
    }))
    runner.assert(dem ~= nil, "failed to create nisar-L2 dataset", true)

    local nanCount = 0

    local starttime = time.latch();
    local batchResults, err = dem:batchsample(lons, lats, heights)

    runner.assert(err == 0, string.format("failed to sample dem: %d", err), true)
    runner.assert(batchResults ~= nil, "failed to create table of samples", true)

    local sampledCount = 0
    for i = 1, #batchResults do
        local val = batchResults[i]
        if val == nil or val ~= val then
            nanCount = nanCount + 1
        else
            sampledCount = sampledCount + 1
        end
    end
    local stoptime = time.latch()
    local dtime = stoptime - starttime
    print(string.format("Batch sampling %d points, valid samples %d, NANs %d, %.3f seconds", numPoints, sampledCount, nanCount, dtime))

end)

-- Report Results --

runner.report()
