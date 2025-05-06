local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")

-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Setup --

local failedSamples = 0

-- Generate arrays for lons, lats, and heights
local function generatePointArrays(startLon, stopLon, lat, height, maxPoints)
    local lons = {}
    local lats = {}
    local heights = {}

    local step = (stopLon - startLon) / (maxPoints - 1)
    for i = 1, maxPoints do
        table.insert(lons, startLon + (i - 1) * step)
        table.insert(lats, lat)
        table.insert(heights, height)
    end

    return lons, lats, heights
end

local verbose = true
local failedSamplesSerial = 0
local failedSamplesBatch = 0

local lat = 66.34 -- Arctic Circle latitude
local height = 0  -- Always zero for this test

local startLon  = 100.0
local stopLon   = 110.0
local maxPoints = 10^5 -- Total number of points

local lons, lats, heights = generatePointArrays(startLon, stopLon, lat, height, maxPoints)

print(string.format("\n------------------------------------\nMosaics serial reading %d points along arctic circle, longitude range (%.2f to %.2f)\n------------------------------------", maxPoints, startLon, stopLon))

-- Capture results from serial sampling
local serialResults = {}
local dem = geo.raster(geo.parms({ asset = "arcticdem-mosaic", algorithm = "NearestNeighbour", radius = 0 }))

local starttimeSerial = time.latch()
local intervaltime = starttimeSerial
local modulovalue = maxPoints / 20

for i = 1, maxPoints do
    local tbl, err = dem:sample(lons[i], lats[i], heights[i])
    if err ~= 0 then
        failedSamplesSerial = failedSamplesSerial + 1
        print(string.format("Serial: Point: %d, (%.2f, %.2f) ======> FAILED to read", i, lons[i], lats[i]))
        table.insert(serialResults, nil)
    else
        table.insert(serialResults, tbl)
    end

    if verbose then
        if (i % modulovalue == 0) then
            local midtime = time.latch()
            local dtime = midtime - intervaltime
            local firstSample = tbl[1]
            local el = firstSample["value"]
            print(string.format("Point: %7d sampled at (%.2f, %.2f), elevation: %7.2fm, %d points interval time: %5.2f", i, lons[i], lats[i], el, modulovalue, dtime))
            intervaltime = time.latch()
        end
    end
end

local stoptimeSerial = time.latch()
local dtimeSerial = stoptimeSerial - starttimeSerial
print(string.format("Serial sampling: %d points read, time: %f, failed reads: %d", maxPoints, dtimeSerial, failedSamplesSerial))

-- Capture results from batch sampling
local batchResults = {}
local starttimeBatch = time.latch()

print(string.format("\n------------------------------------\nMosaics batch reading %d points along arctic circle, longitude range (%.2f to %.2f)\n------------------------------------", maxPoints, startLon, stopLon))
local tbl, err = dem:batchsample(lons, lats, heights)
if err ~= 0 then
    print("Batch sampling failed")
else
    batchResults = tbl
end

local stoptimeBatch = time.latch()
local dtimeBatch = stoptimeBatch - starttimeBatch
print(string.format("Batch sampling: %d points read, time: %f, failed reads: %d", maxPoints, dtimeBatch, failedSamplesBatch))

-- Compare serial and batch results
print("\n------------------------------------\nComparing Serial and Batch Results\n------------------------------------")
local totalMismatches = 0
local totalFailedSerial = 0
local totalFailedBatch = 0

for i = 1, maxPoints do
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

-- Print summary
print("\n------------------------------------\nPerformance Summary\n------------------------------------")
print(string.format("Total Points: %d", maxPoints))
print(string.format("Serial Sampling Time: %6.2f seconds", dtimeSerial))
print(string.format("Batch  Sampling Time: %6.2f seconds", dtimeBatch))
print(string.format("Speedup (Serial/Batch): %.2fx", dtimeSerial / dtimeBatch))
print(string.format("Failed Points (Serial): %d", totalFailedSerial))
print(string.format("Failed Points (Batch):  %d", totalFailedBatch))
print(string.format("Mismatched Points:      %d", totalMismatches))

sys.quit()