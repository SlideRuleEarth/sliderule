local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate()

local geojsonfile = dirpath.."../data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f, "failed to open geojson file")
if not f then return end
local contents = f:read("*all")
f:close()

-- Self Test --

local  sigma = 1.0e-9

local  lon = -179.0
local  lat = 51.0
local  height = 0

print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI)\n-------------------------------------------------"))

local demType = "landsat-hls"

local expResults = {{-0.259439707674, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDVI"}'},
                    { 0.533333333333, 1293837135.0, 'HLS.S30.T01UCS.2021004T230941.v2.0 {"algo": "NDVI"}'},
                    { 0.017137180230, 1294009336.0, 'HLS.S30.T01UCS.2021006T225929.v2.0 {"algo": "NDVI"}'},
                    { 0.011408052369, 1294269135.0, 'HLS.S30.T01UCS.2021009T230929.v2.0 {"algo": "NDVI"}'},
                    { 0.093959731544, 1294441339.0, 'HLS.S30.T01UCS.2021011T225921.v2.0 {"algo": "NDVI"}'},
                    { 0.756097560976, 1294701137.0, 'HLS.S30.T01UCS.2021014T230921.v2.0 {"algo": "NDVI"}'},
                    { 0.026548672566, 1294873337.0, 'HLS.S30.T01UCS.2021016T225909.v2.0 {"algo": "NDVI"}'},
                    {-0.022610551591, 1295565136.0, 'HLS.S30.T01UCS.2021024T230841.v2.0 {"algo": "NDVI"}'},
                    {-1.105263157895, 1295737338.0, 'HLS.S30.T01UCS.2021026T225819.v2.0 {"algo": "NDVI"}'}}

local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDVI"}, catalog = contents, sort_by_index = true}))
local sampleCnt = 0
local ndvi = 0
local gpsTime = 0
local tbl, err = dem:sample(lon, lat, height)
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, _time
    for i, v in ipairs(tbl) do
        value = v["value"]
        _time = v["time"]
        fname = v["file"]
        print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, _time, fname))
        sampleCnt = sampleCnt + 1

        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
end
runner.assert(sampleCnt == #expResults)
dem=nil

expResults = {{ 0.065173116090, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDSI"}'},
              {-0.259439707674, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDVI"}'},
              {-0.203145478375, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDWI"}'},
              { 0.114285714286, 1293837135.0, 'HLS.S30.T01UCS.2021004T230941.v2.0 {"algo": "NDSI"}'},
              { 0.533333333333, 1293837135.0, 'HLS.S30.T01UCS.2021004T230941.v2.0 {"algo": "NDVI"}'},
              {-0.603448275862, 1293837135.0, 'HLS.S30.T01UCS.2021004T230941.v2.0 {"algo": "NDWI"}'},
              {-0.047725899715, 1294009336.0, 'HLS.S30.T01UCS.2021006T225929.v2.0 {"algo": "NDSI"}'},
              { 0.017137180230, 1294009336.0, 'HLS.S30.T01UCS.2021006T225929.v2.0 {"algo": "NDVI"}'},
              { 0.005977237370, 1294009336.0, 'HLS.S30.T01UCS.2021006T225929.v2.0 {"algo": "NDWI"}'},
              { 0.480214002398, 1294269135.0, 'HLS.S30.T01UCS.2021009T230929.v2.0 {"algo": "NDSI"}'},
              { 0.011408052369, 1294269135.0, 'HLS.S30.T01UCS.2021009T230929.v2.0 {"algo": "NDVI"}'},
              { 0.477005893545, 1294269135.0, 'HLS.S30.T01UCS.2021009T230929.v2.0 {"algo": "NDWI"}'},
              {-1.722222222222, 1294441339.0, 'HLS.S30.T01UCS.2021011T225921.v2.0 {"algo": "NDSI"}'},
              { 0.093959731544, 1294441339.0, 'HLS.S30.T01UCS.2021011T225921.v2.0 {"algo": "NDVI"}'},
             {-10.878787878788, 1294441339.0, 'HLS.S30.T01UCS.2021011T225921.v2.0 {"algo": "NDWI"}'},
              { 1.400000000000, 1294701137.0, 'HLS.S30.T01UCS.2021014T230921.v2.0 {"algo": "NDSI"}'},
              { 0.756097560976, 1294701137.0, 'HLS.S30.T01UCS.2021014T230921.v2.0 {"algo": "NDVI"}'},
              { 1.250000000000, 1294701137.0, 'HLS.S30.T01UCS.2021014T230921.v2.0 {"algo": "NDWI"}'},
              {-1.834437086093, 1294873337.0, 'HLS.S30.T01UCS.2021016T225909.v2.0 {"algo": "NDSI"}'},
              { 0.026548672566, 1294873337.0, 'HLS.S30.T01UCS.2021016T225909.v2.0 {"algo": "NDVI"}'},
              {-1.743589743590, 1294873337.0, 'HLS.S30.T01UCS.2021016T225909.v2.0 {"algo": "NDWI"}'},
              { 0.591664149804, 1295565136.0, 'HLS.S30.T01UCS.2021024T230841.v2.0 {"algo": "NDSI"}'},
              {-0.022610551591, 1295565136.0, 'HLS.S30.T01UCS.2021024T230841.v2.0 {"algo": "NDVI"}'},
              { 0.566435061464, 1295565136.0, 'HLS.S30.T01UCS.2021024T230841.v2.0 {"algo": "NDWI"}'},
              {-2.333333333333, 1295737338.0, 'HLS.S30.T01UCS.2021026T225819.v2.0 {"algo": "NDSI"}'},
              {-1.105263157895, 1295737338.0, 'HLS.S30.T01UCS.2021026T225819.v2.0 {"algo": "NDVI"}'},
              {-0.965811965812, 1295737338.0, 'HLS.S30.T01UCS.2021026T225819.v2.0 {"algo": "NDWI"}'}}

print(string.format("\n-------------------------------------------------\nLandsat Plugin test (NDVI, NDSI, NDWI)\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
sampleCnt = 0
local ndviCnt= 0
local ndsiCnt= 0
local ndwiCnt= 0
tbl, err = dem:sample(lon, lat, height)
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, _time
    for i, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        _time  = v["time"]

        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, _time, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(fname == expResults[i][3])

    end
end
runner.assert(sampleCnt == #expResults)
dem=nil



expResults = {{ 0.065173116090, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDSI"}'},
              {-0.259439707674, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDVI"}'},
              {-0.203145478375, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDWI"}'}}

local url = "HLS.S30.T01UCS.2021001T22594"
print(string.format("\n--------------------------------\nTest: %s URL/GROUP Filter: %s\n--------------------------------", demType, url))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, substr = url, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
sampleCnt = 0
local invalidSamples = 0

tbl, err = dem:sample(lon, lat, height)
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, _time
    for i, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        _time  = v["time"]

        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, _time, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
end
runner.assert(sampleCnt == #expResults)
dem=nil



local tstr = "2021:2:4:23:3:0"
local expectedGroup = "T01UCS.2021026T225819.v2.0"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter: closest_time=%s\n--------------------------------", demType, tstr))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true,  closest_time=tstr, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
runner.assert(dem ~= nil)
tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

sampleCnt = 0
local value, fname
for i, v in ipairs(tbl) do
    fname = v["file"]
    value = v["value"]
    print(string.format("(%02d) value %16.12f, fname: %s", i, value, fname))
    runner.assert( string.find(fname, expectedGroup))
    sampleCnt = sampleCnt + 1
end
runner.assert(sampleCnt == 3)  -- 3 groups with closest time
dem = nil


local tstr          = "2021:2:4:23:3:0"
local tstrOverride  = "2000:2:4:23:3:0"
local expectedGroup = "T01UCS.2021001T225941.v2.0"

print(string.format("\n--------------------------------\nTest: %s Temporal Filter Override:  closest_time=%s\n--------------------------------", demType, tstrOverride, tstr))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, closest_time=tstr, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
runner.assert(dem ~= nil)
tbl, err = dem:sample(lon, lat, height, tstrOverride)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

sampleCnt = 0
local value, fname
for i, v in ipairs(tbl) do
    fname = v["file"]
    value = v["value"]
    print(string.format("(%02d) value %16.12f, fname: %s", i, value, fname))
    runner.assert( string.find(fname, expectedGroup))
    sampleCnt = sampleCnt + 1
end
runner.assert(sampleCnt == 3)  -- 3 groups with closest time
dem = nil



expResults = {{  523.0, 1293577339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021001T225941.v2.0/HLS.S30.T01UCS.2021001T225941.v2.0.B03.tif'},
              {  117.0, 1293837135.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021004T230941.v2.0/HLS.S30.T01UCS.2021004T230941.v2.0.B03.tif'},
              { 5517.0, 1294009336.0, 0x42, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021006T225929.v2.0/HLS.S30.T01UCS.2021006T225929.v2.0.B03.tif'},
              {16047.0, 1294269135.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021009T230929.v2.0/HLS.S30.T01UCS.2021009T230929.v2.0.B03.tif'},
              {  -52.0, 1294441339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021011T225921.v2.0/HLS.S30.T01UCS.2021011T225921.v2.0.B03.tif'},
              {  -24.0, 1294701137.0, 0x60, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021014T230921.v2.0/HLS.S30.T01UCS.2021014T230921.v2.0.B03.tif'},
              {  -63.0, 1294873337.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021016T225909.v2.0/HLS.S30.T01UCS.2021016T225909.v2.0.B03.tif'},
              { 7905.0, 1295565136.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021024T230841.v2.0/HLS.S30.T01UCS.2021024T230841.v2.0.B03.tif'},
              {  -46.0, 1295737338.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021026T225819.v2.0/HLS.S30.T01UCS.2021026T225819.v2.0.B03.tif'}}


print(string.format("\n-------------------------------------------------\nLandsat Plugin test (BO3 and qualit flags)\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, bands = {"B03"}, catalog = contents, sort_by_index = true }))
local fmaskCnt  = 0
local sampleCnt = 0
local b03 = 0
tbl, err = dem:sample(lon, lat, height)
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, _time, flags
    for i, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        _time  = v["time"]
        flags = v["flags"]
        sampleCnt = sampleCnt + 1

        print(string.format("(%02d) value %10.1f, time %10.1f, qmask 0x%x, %s", i, value, _time, flags, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(flags == expResults[i][3])
        runner.assert(fname == expResults[i][4])
    end
end
runner.assert(sampleCnt == #expResults)
dem = nil




expResults = {{  523.0, 1293577339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021001T225941.v2.0/HLS.S30.T01UCS.2021001T225941.v2.0.B03.tif'},
              {  194.0, 1293577339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021001T225941.v2.0/HLS.S30.T01UCS.2021001T225941.v2.0.Fmask.tif'},
              {  117.0, 1293837135.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021004T230941.v2.0/HLS.S30.T01UCS.2021004T230941.v2.0.B03.tif'},
              {  224.0, 1293837135.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021004T230941.v2.0/HLS.S30.T01UCS.2021004T230941.v2.0.Fmask.tif'},
              { 5517.0, 1294009336.0, 0x42, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021006T225929.v2.0/HLS.S30.T01UCS.2021006T225929.v2.0.B03.tif'},
              {   66.0, 1294009336.0, 0x42, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021006T225929.v2.0/HLS.S30.T01UCS.2021006T225929.v2.0.Fmask.tif'},
              {16047.0, 1294269135.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021009T230929.v2.0/HLS.S30.T01UCS.2021009T230929.v2.0.B03.tif'},
              {  194.0, 1294269135.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021009T230929.v2.0/HLS.S30.T01UCS.2021009T230929.v2.0.Fmask.tif'},
              {  -52.0, 1294441339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021011T225921.v2.0/HLS.S30.T01UCS.2021011T225921.v2.0.B03.tif'},
              {  194.0, 1294441339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021011T225921.v2.0/HLS.S30.T01UCS.2021011T225921.v2.0.Fmask.tif'},
              {  -24.0, 1294701137.0, 0x60, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021014T230921.v2.0/HLS.S30.T01UCS.2021014T230921.v2.0.B03.tif'},
              {   96.0, 1294701137.0, 0x60, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021014T230921.v2.0/HLS.S30.T01UCS.2021014T230921.v2.0.Fmask.tif'},
              {  -63.0, 1294873337.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021016T225909.v2.0/HLS.S30.T01UCS.2021016T225909.v2.0.B03.tif'},
              {  194.0, 1294873337.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021016T225909.v2.0/HLS.S30.T01UCS.2021016T225909.v2.0.Fmask.tif'},
              { 7905.0, 1295565136.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021024T230841.v2.0/HLS.S30.T01UCS.2021024T230841.v2.0.B03.tif'},
              {  194.0, 1295565136.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021024T230841.v2.0/HLS.S30.T01UCS.2021024T230841.v2.0.Fmask.tif'},
              {  -46.0, 1295737338.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021026T225819.v2.0/HLS.S30.T01UCS.2021026T225819.v2.0.B03.tif'},
              {  224.0, 1295737338.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021026T225819.v2.0/HLS.S30.T01UCS.2021026T225819.v2.0.Fmask.tif'}}


print(string.format("\n-------------------------------------------------\nLandsat Sample test (BO3 and Fmask raster)\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, bands = {"B03", "Fmask"}, catalog = contents, sort_by_index = true }))
fmaskCnt  = 0
sampleCnt = 0
b03 = 0
tbl, err = dem:sample(lon, lat, height)
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, flags
    for i, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        _time  = v["time"]
        flags = v["flags"]
        sampleCnt = sampleCnt + 1

        print(string.format("(%02d) value %10.1f, time %10.1f, qmask 0x%x, %s", i, value, _time, flags, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(flags == expResults[i][3])
        runner.assert(fname == expResults[i][4])
    end
end
runner.assert(sampleCnt == #expResults)
dem = nil


print(string.format("\n-------------------------------------------------\nLandsat POI Sample many rasters test\n-------------------------------------------------"))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0,
                            bands = {"VAA", "VZA", "Fmask","SAA", "SZA", "NDSI", "NDVI", "NDWI",
                                     "B01", "B02", "B03", "B04", "B05", "B06",
                                     "B07", "B08", "B09", "B10", "B11", "B12", "B8A"},
                            catalog = contents, sort_by_index = true }))

sampleCnt = 0
local starttime = time.latch();
tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        sampleCnt = sampleCnt + 1
        -- print(string.format("(%02d) value %10.3f, fname: %s", j, value, fname))
    end
end
runner.assert(sampleCnt == 180)
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, sampleCnt))


--[[

EL - we are currently not using subseting and this test is very slow (180 rasters to subset)
   - I am disabling it for now. There are other tests (shorter) which subset rasters.
   - I am leaving this here in case we want to re-enable it later.

print(string.format("\n-------------------------------------------------\nLandsat AOI Subset test\n-------------------------------------------------"))

-- AOI extent (extent of hls_trimmed.geojson)
llx =  -179.87
lly =    50.45
urx =  -178.27
ury =    51.44


dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0,
                            bands = {"VAA", "VZA", "Fmask","SAA", "SZA", "NDSI", "NDVI", "NDWI",
                                     "B01", "B02", "B03", "B04", "B05", "B06",
                                     "B07", "B08", "B09", "B10", "B11", "B12", "B8A"},
                            catalog = contents, sort_by_index = true }))

local starttime = time.latch();
local tbl, err = dem:subset(llx, lly, urx, ury)
local stoptime = time.latch();

runner.assert(err == 0)
runner.assert(tbl ~= nil)

local threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

runner.assert(threadCnt == 167)

if tbl ~= nil then
    for i, v in ipairs(tbl) do
        local size = v["size"]
        local mbytes = size / (1024*1024)

        if i == 1 then
            print(string.format("AOI subset datasize: %.1f MB", mbytes))
        end

        runner.assert(size > 0)
    end
end
--]]



-- Grand Mesa test has 26183 samples/ndvi Results
-- Limit the number of samples for selftest.
local maxSamples = 100
local linesRead = 0

local geojsonfile = dirpath.."../data/grand_mesa.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

local poifile = dirpath.."../data/grand_mesa_poi.txt"
local f = io.open(poifile, "r")
-- read in array of POI from file
local arr = {}
for l in f:lines() do
  local row = {}
  for snum in l:gmatch("(%S+)") do
    table.insert(row, tonumber(snum))
  end
  table.insert(arr, row)
  linesRead = linesRead + 1
  if linesRead > maxSamples then
    break
  end
end
f:close()

local ndvifile = dirpath.."../data/grand_mesa_ndvi.txt"
local f = io.open(ndvifile, "r")
-- read in array of NDVI values
local ndvi_results = {}
linesRead = 0

for l in f:lines() do
  table.insert(ndvi_results, tonumber(l))
  linesRead = linesRead + 1
  if linesRead > maxSamples then
    break
  end
end
f:close()

local expectedFile = "HLS.S30.T12SYJ.2022004T180729.v2.0 {\"algo\": \"NDVI\"}"

print(string.format("\n-------------------------------------------------\nLandsat Grand Mesa test\n-------------------------------------------------"))

local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, closest_time = "2022-01-05T00:00:00Z", bands = {"NDVI"}, catalog = contents, sort_by_index = true }))
sampleCnt = 0

local starttime = time.latch();
for i=1, maxSamples do
    local  lon = arr[i][1]
    local  lat = arr[i][2]
    local  tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        print(string.format("======> FAILED to read", lon, lat, height))
    else
        local ndvi, fname
        for j, v in ipairs(tbl) do
            ndvi = v["value"]
            fname = v["file"]

            local expectedNDVI = ndvi_results[i]
            runner.assert(math.abs(ndvi - expectedNDVI) < sigma)
            runner.assert(fname == expectedFile)
        end
    end
    sampleCnt = sampleCnt + 1
end
local stoptime = time.latch();
print(string.format("POI sample %d points time: %.2f", sampleCnt, stoptime - starttime))
runner.assert(sampleCnt == maxSamples)
dem = nil

-- Report Results --

runner.report()
