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

local starttime = time.latch();

local dem = geo.raster(geo.parms({
    asset = "nisar-L2-geoff",
    algorithm = "NearestNeighbour",
    catalog = contents,
    sort_by_index = true
}))
runner.assert(dem ~= nil, "failed to create nisar-L2 dataset", true)

local nanCount = 0
local totalCount = 0
local failedSamples = 0

local serialResults = {}

for j = 1, numPoints do
    local lon = lons[j]
    local lat = lats[j]
    local height = heights[j]
    local tbl, err = dem:sample(lon, lat, height)

    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    if err ~= 0 then
        print(string.format("Point: %d, (%.5f, %.5f) ======> FAILED to read, err# %d", j, lon, lat, err))
        failedSamples = failedSamples + 1
        table.insert(serialResults, nil)
    else
        table.insert(serialResults, tbl)

        for k, v in ipairs(tbl) do
            local value = v["value"]
            local fname = v["file"]
            totalCount = totalCount + 1
            if tostring(value) == "nan" then
                nanCount = nanCount + 1
            end
            -- print(string.format("(%.5f, %.5f)  %8.4f", lon, lat, value))
            -- print(string.format("(%.5f, %.5f)  %8.4f  %s\n", lon, lat, value, fname))
        end
    end
end

local stoptime = time.latch()
local dtime = stoptime-starttime
local percentNaN = (nanCount / totalCount) * 100
print(string.format("Serial sampling %d points, %d samples, ExecTime: %f, failed reads: %d, NaN Values: %d (%.2f%%)\n", numPoints, totalCount, dtime, failedSamples, nanCount, percentNaN))


local batchResults = {}
starttime = time.latch();
tbl, err = dem:batchsample(lons, lats, heights)

runner.assert(err == 0)
runner.assert(tbl ~= nil)

if err ~= 0 then
    print(string.format("Point: (%.5f, %.5f) ======> FAILED to read, err# %d", lon, lat, err))
    failedSamples = failedSamples + 1
    table.insert(batchResults, nil)
else
    batchResults = tbl
end

stoptime = time.latch()
dtime = stoptime-starttime
print(string.format("Batch sampling %d points, %d samples, ExecTime: %f\n", numPoints, totalCount, dtime))

-- Compare serial and batch results
print("\n------------------------------------\nComparing Serial and Batch Results\n------------------------------------")
local totalMismatches = 0
local totalFailedSerial = 0
local totalFailedBatch = 0

for i = 1, numPoints do
    local serialSample = serialResults[i]
    local batchSample = batchResults[i]

    if (serialSample == nil and batchSample == nil) then
        -- Both failed
        totalFailedSerial = totalFailedSerial + 1
        totalFailedBatch = totalFailedBatch + 1
        print(string.format("Point %d: Both methods failed", i))
    elseif (serialSample == nil) then
        -- Serial failed
        totalFailedSerial = totalFailedSerial + 1
        totalMismatches = totalMismatches + 1
        print(string.format("Point %d: Serial failed, Batch succeeded", i))
    elseif (batchSample == nil) then
        -- Batch failed
        totalFailedBatch = totalFailedBatch + 1
        totalMismatches = totalMismatches + 1
        print(string.format("Point %d: Batch failed, Serial succeeded", i))
    else
        -- Compare values
        for j, serialValue in ipairs(serialSample) do
            local batchValue = batchSample[j]
            if not batchValue or math.abs(serialValue["value"] - batchValue["value"]) > 1e-6 then
                totalMismatches = totalMismatches + 1
                print(string.format("Point %d, Sample %d: Serial %.6f != Batch %.6f", i, j, serialValue["value"], batchValue["value"]))
            end
        end
    end
end

-- Report Results --
runner.report()


