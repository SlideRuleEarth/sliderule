local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

local sigma = 1.0e-9

local lon =  150.0
local lat =   75.0
local height = 0.0

local demType = "arcticdem-mosaic"
local samplingRadius = 30

print(string.format("\n--------------------------------\nTest: %s Zonal Stats, sampling radius %d meters\n--------------------------------", demType, samplingRadius))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true}))
tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

local expElevation = 19.890625000000
local expMin       = 18.929687500000
local expMax       = 20.335937500000
local expMean      = 19.829359132581
local expMedian    = 19.914062500000
local expStdev     =  0.355630373179
local expMad       =  0.298961002355
local expCnt       = 709

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local cnt = v["count"]
    local min = v["min"]
    local max = v["max"]
    local mean = v["mean"]
    local median = v["median"]
    local stdev = v["stdev"]
    local mad = v["mad"]
    print(string.format("(%02d)  %.12fm   cnt: %d   min: %.12f   max: %.12f   mean: %.12f   median: %.12f   stdev: %.12f   mad: %.12f", i, el, cnt, min, max, mean, median, stdev, mad))
    sampleCnt = sampleCnt + 1

    runner.assert(math.abs(el - expElevation) < sigma)
    runner.assert(math.abs(min - expMin) < sigma)
    runner.assert(math.abs(max - expMax) < sigma)
    runner.assert(math.abs(mean - expMean) < sigma)
    runner.assert(math.abs(median - expMedian) < sigma)
    runner.assert(math.abs(stdev - expStdev) < sigma)
    runner.assert(math.abs(mad - expMad) < sigma)
    runner.assert(cnt == expCnt)
end
dem=nil


--Use much smaller sampling radius
samplingRadius = 10
print(string.format("\n--------------------------------\nTest: %s Zonal Stats, sampling radius %d meters\n--------------------------------", demType, samplingRadius))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true}))
tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

expElevation = 19.890625000000
expMin       = 19.578125000000
expMax       = 20.054687500000
expMean      = 19.845003858025
expMedian    = 19.851562500000
expStdev     =  0.117738908413
expMad       =  0.099748990245
expCnt       = 81

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local cnt = v["count"]
    local min = v["min"]
    local max = v["max"]
    local mean = v["mean"]
    local median = v["median"]
    local stdev = v["stdev"]
    local mad = v["mad"]
    print(string.format("(%02d)  %.12fm   cnt: %d   min: %.12f   max: %.12f   mean: %.12f   median: %.12f   stdev: %.12f   mad: %.12f", i, el, cnt, min, max, mean, median, stdev, mad))
    sampleCnt = sampleCnt + 1

    runner.assert(math.abs(el - expElevation) < sigma)
    runner.assert(math.abs(min - expMin) < sigma)
    runner.assert(math.abs(max - expMax) < sigma)
    runner.assert(math.abs(mean - expMean) < sigma)
    runner.assert(math.abs(median - expMedian) < sigma)
    runner.assert(math.abs(stdev - expStdev) < sigma)
    runner.assert(math.abs(mad - expMad) < sigma)
    runner.assert(cnt == expCnt)
end
dem=nil

-- Report Results --

runner.report()
