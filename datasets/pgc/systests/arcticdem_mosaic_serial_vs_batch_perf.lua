local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

local readgeojson = require("readgeojson")
local jsonfile = td .. "../data/arcticdem_strips.geojson"
local contents = readgeojson.load(jsonfile)

local generator = require("arctictdem_test_points_generator")
local maxPoints = 1000
local lons, lats, heights = generator.generate_points(maxPoints)
local verbose = true

print(string.format("\n------------------------------------\nArcticDEM mosaics serial reading %d points\n------------------------------------", maxPoints))

-- Capture results from serial sampling
local failedSamplesSerial = 0

local serialResults = {}
local dem = geo.raster(geo.parms({ asset = "arcticdem-mosaic", algorithm = "NearestNeighbour", radius = 0 }))

local starttimeSerial = time.latch()
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
            local firstSample = tbl[1]
            local el = firstSample["value"]
            print(string.format("Point: %7d sampled at (%.2f, %.2f), elevation: %7.2fm", i, lons[i], lats[i], el))
        end
    end
end

local dtimeSerial = time.latch() - starttimeSerial
print(string.format("%d points read, time: %f, failed reads: %d", maxPoints, dtimeSerial, failedSamplesSerial))

-- Capture results from batch sampling
local batchResults = {}
local starttimeBatch = time.latch()

print(string.format("\n------------------------------------\nArcticDEM mosaics batch reading %d points\n------------------------------------", maxPoints))
local tbl, err = dem:batchsample(lons, lats, heights)
if err ~= 0 then
    print("Batch sampling failed")
else
    batchResults = tbl
end

local stoptimeBatch = time.latch()
local dtimeBatch = stoptimeBatch - starttimeBatch
print(string.format("Batch sampling: %d points read, time: %f", maxPoints, dtimeBatch))

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