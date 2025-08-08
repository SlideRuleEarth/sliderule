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
local maxPoints = 5
local lons, lats, heights = generator.generate_points(maxPoints)
local verbose = true

local sample_fileio = require("samples_fileio")

print(string.format("\n------------------------------------\nArcticDEM strips serial reading %d points\n------------------------------------", maxPoints))

local failedSamplesSerial = 0
local failedSamplesBatch = 0

local function is_empty(tbl)
    return next(tbl) == nil
end


-- Capture results from serial sampling
local serialResults = {}
local dem = geo.raster(geo.parms({ asset = "arcticdem-strips", catalog = contents, algorithm = "NearestNeighbour", radius = 0, sort_by_index = true }))

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
            if not is_empty(tbl) then
                local firstSample = tbl[1]
                local el = firstSample["value"]
                print(string.format("Point: %7d sampled at (%.2f, %.2f), elevation: %7.2fm", i, lons[i], lats[i], el))
            else
                print(string.format("Point: %7d sampled at (%.2f, %.2f), elevation: EMPTY_TABLE", i, lons[i], lats[i]))
            end
        end
    end
end

local stoptimeSerial = time.latch()
local dtimeSerial = stoptimeSerial - starttimeSerial
print(string.format("Serial sampling: %d points read, time: %f, failed reads: %d", maxPoints, dtimeSerial, failedSamplesSerial))

-- Capture results from batch sampling
local batchResults = {}
local starttimeBatch = time.latch()

print(string.format("\n------------------------------------\nArcticDEM stript batch reading %d points\n------------------------------------", maxPoints))
local tbl, err = dem:batchsample(lons, lats, heights)
if err ~= 0 then
    print("Batch sampling failed")
else
    batchResults = tbl
end

local dtimeBatch = time.latch() - starttimeBatch
print(string.format("Batch sampling: %d points read, time: %f, failed reads: %d", maxPoints, dtimeBatch, failedSamplesBatch))

local fileio = require("samples_fileio")
local mismatches = fileio.compare_samples_tables(serialResults, batchResults)
print(string.format("Detected %d mismatches", mismatches))

-- Print summary
print("\n------------------------------------\nPerformance Summary\n------------------------------------")
print(string.format("Total Points: %d", maxPoints))
print(string.format("Serial Sampling Time: %6.2f seconds", dtimeSerial))
print(string.format("Batch  Sampling Time: %6.2f seconds", dtimeBatch))
print(string.format("Speedup (Serial/Batch): %.2fx", dtimeSerial / dtimeBatch))

-- Report Results --
runner.report()