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
local maxPoints = 1000  -- Do not change this value, the generated points are fixed and samples are deterministic
local lons, lats, heights = generator.generate_points(maxPoints)

local t0str = "2014:2:25:23:00:00" -- limit number of strips per point to this temporal range
local t1str = "2016:6:2:24:00:00"
print(string.format("\n------------------------------------\nArcticDEM strips sampling %d points\n------------------------------------", maxPoints))
local starttime = time.latch()
local dem = geo.raster(geo.parms({ asset = "arcticdem-strips", algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, t0=t0str, t1=t1str, sort_by_index=true}))
local tbl, err = dem:batchsample(lons, lats, heights)
if err ~= 0 then
    print("Batch sampling failed")
else
    batchResults = tbl
end

local dtime = time.latch() - starttime
print(string.format("Batch sampling: %d points read, time: %f", maxPoints, dtime))


local fileio = require("samples_fileio")
local luatable = td .. "../data/arcticdem_strips_results_1k_points.lua.gz"
expectedSamples = fileio.load_results_table(luatable)

local mismatches = fileio.compare_samples_tables(expectedSamples, tbl)
print(string.format("Detected %d mismatches", mismatches))

-- Report Results --

runner.report()