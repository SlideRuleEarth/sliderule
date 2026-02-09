    local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'lpdaac-cloud'})

-- read trimmed hls geojson
local geojsonfile = dirpath.."../data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f, "failed to open geojson file", true)
local contents = f:read("*all")
f:close()

-- raster to sample
local demType = "landsat-hls"

-- tolerance
local  sigma = 1.0e-9

-- point of interest
local  lon = -179.0
local  lat = 51.0
local  height = 0

-- Self Test --

runner.uittest("Landsat Test (NDVI)", function()
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
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    for i, v in ipairs(tbl) do
        local value = v["value"]
        local _time = v["time"]
        local fname = v["file"]
        print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, _time, fname))
        sampleCnt = sampleCnt + 1

        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
    runner.assert(sampleCnt == #expResults)
end)

-- Self Test --

runner.uittest("Landsat Test (NDVI, NDSI, NDWI)", function()
    local expResults = {{ 0.065173116090, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDSI"}'},
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
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
    local sampleCnt = 0
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    for i, v in ipairs(tbl) do
        local value = v["value"]
        local fname = v["file"]
        local _time  = v["time"]
        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, _time, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(fname == expResults[i][3])

    end
    runner.assert(sampleCnt == #expResults)
end)

-- Self Test --

runner.uittest("Landsat URL/GROUP Filter", function()
    local expResults = {{ 0.065173116090, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDSI"}'},
                        {-0.259439707674, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDVI"}'},
                        {-0.203145478375, 1293577339.0, 'HLS.S30.T01UCS.2021001T225941.v2.0 {"algo": "NDWI"}'}}
    local url = "HLS.S30.T01UCS.2021001T22594"
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, substr = url, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
    local sampleCnt = 0
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    for i, v in ipairs(tbl) do
        local value = v["value"]
        local fname = v["file"]
        local _time  = v["time"]
        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %16.12f, time %.2f, fname: %s", i, value, _time, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
    runner.assert(sampleCnt == #expResults)
end)

-- Self Test --

runner.uittest("Landsat Temporal Filter: closest_time", function()
    local tstr = "2021:2:4:23:3:0"
    local expectedGroup = "T01UCS.2021026T225819.v2.0"
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true,  closest_time=tstr, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true }))
    runner.assert(dem ~= nil, "failed to create raster", true)
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    local sampleCnt = 0
    for i, v in ipairs(tbl) do
        local fname = v["file"]
        local value = v["value"]
        print(string.format("(%02d) value %16.12f, fname: %s", i, value, fname))
        runner.assert( string.find(fname, expectedGroup))
        sampleCnt = sampleCnt + 1
    end
    runner.assert(sampleCnt == 3)  -- 3 groups with closest time
end)

-- Self Test --

runner.uittest("Landsat Temporal Filter Override: closest_time", function()
    local tstr          = "2021:2:4:23:3:0"
    local tstrOverride  = "2000:2:4:23:3:0"
    local expectedGroup = "T01UCS.2021001T225941.v2.0"
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, closest_time=tstr, bands = {"NDSI", "NDVI", "NDWI"}, catalog = contents, sort_by_index = true, use_poi_time=true}))
    runner.assert(dem ~= nil, "failed to create raster", true)
    local tbl, err = dem:sample(lon, lat, height, tstrOverride)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    local sampleCnt = 0
    for i, v in ipairs(tbl) do
        local fname = v["file"]
        local value = v["value"]
        print(string.format("(%02d) value %16.12f, fname: %s", i, value, fname))
        runner.assert( string.find(fname, expectedGroup))
        sampleCnt = sampleCnt + 1
    end
    runner.assert(sampleCnt == 3)  -- 3 groups with closest time
end)

-- Self Test --

runner.uittest("Landsat Test (BO3 and quality flags)", function()
    local expResults = {{  523.0, 1293577339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021001T225941.v2.0/HLS.S30.T01UCS.2021001T225941.v2.0.B03.tif'},
                        {  117.0, 1293837135.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021004T230941.v2.0/HLS.S30.T01UCS.2021004T230941.v2.0.B03.tif'},
                        { 5517.0, 1294009336.0, 0x42, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021006T225929.v2.0/HLS.S30.T01UCS.2021006T225929.v2.0.B03.tif'},
                        {16047.0, 1294269135.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021009T230929.v2.0/HLS.S30.T01UCS.2021009T230929.v2.0.B03.tif'},
                        {  -52.0, 1294441339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021011T225921.v2.0/HLS.S30.T01UCS.2021011T225921.v2.0.B03.tif'},
                        {  -24.0, 1294701137.0, 0x60, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021014T230921.v2.0/HLS.S30.T01UCS.2021014T230921.v2.0.B03.tif'},
                        {  -63.0, 1294873337.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021016T225909.v2.0/HLS.S30.T01UCS.2021016T225909.v2.0.B03.tif'},
                        { 7905.0, 1295565136.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021024T230841.v2.0/HLS.S30.T01UCS.2021024T230841.v2.0.B03.tif'},
                        {  -46.0, 1295737338.0, 0xe0, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021026T225819.v2.0/HLS.S30.T01UCS.2021026T225819.v2.0.B03.tif'}}
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, bands = {"B03"}, catalog = contents, sort_by_index = true }))
    local sampleCnt = 0
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    for i, v in ipairs(tbl) do
        local value = v["value"]
        local fname = v["file"]
        local _time  = v["time"]
        local flags = v["flags"]
        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %10.1f, time %10.1f, qmask 0x%x, %s", i, value, _time, flags, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(flags == expResults[i][3])
        runner.assert(fname == expResults[i][4])
    end
    runner.assert(sampleCnt == #expResults)
end)

-- Self Test --

runner.uittest("Landsat Sample Test (BO3 and Fmask raster)", function()
    local expResults = {{  523.0, 1293577339.0, 0xc2, '/vsis3/lp-prod-protected/HLSS30.020/HLS.S30.T01UCS.2021001T225941.v2.0/HLS.S30.T01UCS.2021001T225941.v2.0.B03.tif'},
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
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, with_flags=true, bands = {"B03", "Fmask"}, catalog = contents, sort_by_index = true }))
    local sampleCnt = 0
    local tbl, err = dem:sample(lon, lat, height)
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    for i, v in ipairs(tbl) do
        local value = v["value"]
        local fname = v["file"]
        local _time  = v["time"]
        local flags = v["flags"]
        sampleCnt = sampleCnt + 1
        print(string.format("(%02d) value %10.1f, time %10.1f, qmask 0x%x, %s", i, value, _time, flags, fname))
        runner.assert(math.abs(value - expResults[i][1]) < sigma)
        runner.assert(_time == expResults[i][2])
        runner.assert(flags == expResults[i][3])
        runner.assert(fname == expResults[i][4])
    end
    runner.assert(sampleCnt == #expResults)
end)

-- Self Test --

runner.uittest("Landsat POI Sample Many Rasters Test", function()
    local dem = geo.raster(geo.parms({
        asset = demType,
        algorithm = "NearestNeighbour",
        radius = 0,
        bands = { "VAA", "VZA", "Fmask","SAA", "SZA", "NDSI", "NDVI", "NDWI",
                  "B01", "B02", "B03", "B04", "B05", "B06",
                  "B07", "B08", "B09", "B10", "B11", "B12", "B8A"},
        catalog = contents,
        sort_by_index = true}))
    local sampleCnt = 0
    local starttime = time.latch();
    local tbl, err = dem:sample(lon, lat, height)
    local stoptime = time.latch();
    runner.assert(err == 0, string.format("======> FAILED to read", lon, lat), true)
    for _,_ in ipairs(tbl) do
        sampleCnt = sampleCnt + 1
    end
    runner.assert(sampleCnt == 180)
    print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, sampleCnt))
end)

-- Self Test --

runner.uittest("Landsat Grand Mesa Test", function()
    -- Grand Mesa test has 26183 samples/ndvi Results
    -- Limit the number of samples for selftest.
    local maxSamples = 100
    local linesRead = 0

    -- read grand mesa area of interest file
    local aoi_f = io.open(dirpath.."../data/grand_mesa.geojson", "r")
    assert(aoi_f, "failed to open area of interest file")
    local aoi_catalog = aoi_f:read("*all")
    aoi_f:close()

    -- read in array of POI from file
    local poi_f = io.open(dirpath.."../data/grand_mesa_poi.txt", "r")
    assert(poi_f, "failed to open point of interest file")
    local arr = {}
    for l in poi_f:lines() do
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
    poi_f:close()

    -- read in array of NDVI values
    local ndvi_f = io.open(dirpath.."../data/grand_mesa_ndvi.txt", "r")
    assert(ndvi_f, "failed to open ndvi file")
    local ndvi_results = {}
    linesRead = 0
    for l in ndvi_f:lines() do
        table.insert(ndvi_results, tonumber(l))
        linesRead = linesRead + 1
        if linesRead > maxSamples then
            break
        end
    end
    ndvi_f:close()

    -- sample points
    local expectedFile = "HLS.S30.T12SYJ.2022004T180729.v2.0 {\"algo\": \"NDVI\"}"
    local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, closest_time = "2022-01-05T00:00:00Z", bands = {"NDVI"}, catalog = aoi_catalog, sort_by_index = true }))
    local sampleCnt = 0
    local starttime = time.latch();
    for i=1, maxSamples do
        local  lon_i = arr[i][1]
        local  lat_i = arr[i][2]
        local  tbl, err = dem:sample(lon_i, lat_i, height)
        runner.assert(err == 0, string.format("======> FAILED to read", lon_i, lat_i), true)
        local ndvi, fname
        for _, v in ipairs(tbl) do
            ndvi = v["value"]
            fname = v["file"]
            local expectedNDVI = ndvi_results[i]
            runner.assert(math.abs(ndvi - expectedNDVI) < sigma)
            runner.assert(fname == expectedFile)
        end
        sampleCnt = sampleCnt + 1
    end
    local stoptime = time.latch();
    print(string.format("POI sample %d points time: %.2f", sampleCnt, stoptime - starttime))
    runner.assert(sampleCnt == maxSamples)
end)

-- Report Results --

runner.report()
