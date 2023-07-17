local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test For Temporal filter --

local sigma = 1.0e-9

local lon =  150.0
local lat =   75.0
local height = 0.0

local demType = "arcticdem-mosaic"
local samplingRadius = 30

print(string.format("\n--------------------------------\nTest: %s Zonal Stats, sampling radius %d meters\n--------------------------------", demType, samplingRadius))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true}))
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

local expElevation = 19.812837600708
local expMin       = 18.918535232544
local expMax       = 20.307407379150
local expMean      = 19.765134437464
local expMedian    = 19.833356857300
local expStdev     =  0.331736795754
local expMad       =  0.274090053286
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

    runner.check(math.abs(el - expElevation) < sigma)
    runner.check(math.abs(min - expMin) < sigma)
    runner.check(math.abs(max - expMax) < sigma)
    runner.check(math.abs(mean - expMean) < sigma)
    runner.check(math.abs(median - expMedian) < sigma)
    runner.check(math.abs(stdev - expStdev) < sigma)
    runner.check(math.abs(mad - expMad) < sigma)
    runner.check(cnt == expCnt)
end
dem=nil


--Use much smaller sampling radius
samplingRadius = 10
print(string.format("\n--------------------------------\nTest: %s Zonal Stats, sampling radius %d meters\n--------------------------------", demType, samplingRadius))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=samplingRadius, zonal_stats=true}))
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

expElevation = 19.812837600708
expMin       = 19.567863464355
expMax       = 19.988794326782
expMean      = 19.796006614779
expMedian    = 19.802295684814
expStdev     =  0.102205825908
expMad       =  0.084735066257
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

    runner.check(math.abs(el - expElevation) < sigma)
    runner.check(math.abs(min - expMin) < sigma)
    runner.check(math.abs(max - expMax) < sigma)
    runner.check(math.abs(mean - expMean) < sigma)
    runner.check(math.abs(median - expMedian) < sigma)
    runner.check(math.abs(stdev - expStdev) < sigma)
    runner.check(math.abs(mad - expMad) < sigma)
    runner.check(cnt == expCnt)
end
dem=nil

-- Report Results --

runner.report()

