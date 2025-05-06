local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- AOI extent (extent of grandmesa.geojson)
local gm_llx = -108.3412
local gm_lly =   38.8236
local gm_urx = -107.7292
local gm_ury =   39.1956

local sigma = 1.0e-9

local demType = "arcticdem-strips"
print(string.format("\n------------------------------\n%s Threads Error\n------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

-- AOI extent
local llx =  -150.0
local lly =   70.0
local urx =  -145.0
local ury =   85.0


starttime = time.latch();
tbl, err = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 0) --Error should happen, asking for too much memory or too many threads for strips

--Expecting 'mempool throw' for strips, let it print correctly
sys.wait(1)


local demType = "arcticdem-strips"
print(string.format("\n------------------------------\n%s Memory Error\n------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

-- AOI extent
local llx =  -148.50
local lly =   70.50
local urx =  -148.90
local ury =   70.90


starttime = time.latch();
tbl, err = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 0) --Error should happen, asking for too much memory or too many threads for strips

--Expecting 'mempool throw' for strips, let it print correctly
sys.wait(1)


local demType = "arcticdem-mosaic"
print(string.format("\n------------------------------\n%s Memory Error\n------------------------------", demType))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

-- AOI extent
local llx =  -145.81
local lly =   70.00
local urx =  -150.00
local ury =   71.00

starttime = time.latch();
tbl, err = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 0) --Error should happen, asking for too much memory

--Expecting 'throw' let it print correctly
sys.wait(1)


demTypes = {"rema-mosaic", "rema-strips"}
for i = 1, #demTypes do

    -- AOI extent
    llx =  149.80
    lly =  -70.00
    urx =  150.01
    ury =  -69.93

    demType = demTypes[i];
    print(string.format("\n--------------------------\n%s\n--------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
    starttime = time.latch();
    tbl, err = dem:sample(llx, lly, 0)
    stoptime = time.latch();

    -- For mosaics this is too much memory
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    threadCnt = 0
    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        -- print(string.format("(%02d) %10.2f  %s", i, el, fname))
        threadCnt = threadCnt + 1
    end
    print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if demType == "rema-mosaic" then
        runner.assert(threadCnt == 1)
    else
        runner.assert(threadCnt == 10)
    end

    starttime = time.latch();
    tbl, err = dem:subset(llx, lly, urx, ury)
    stoptime = time.latch();

    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    threadCnt = 0
    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            threadCnt = threadCnt + 1
        end
    end
    print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if demType == "rema-mosaic" then
        runner.assert(threadCnt == 1)
    else
        runner.assert(threadCnt == 20)
    end


    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            local size = v["size"]
            local mbytes = size / (1024*1024)

            -- This results in 20 threads, all the same size, cols, buffs data type. Print only first one
            print(string.format("AOI size: %6.1f MB", mbytes))
            runner.assert(size > 0)
        end
    end
end


demType = "usgs3dep-1meter-dem"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

local geojsonfile = td.."../../plugins/usgs3dep/data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))
starttime = time.latch();
local tbl, err = dem:sample(gm_llx, gm_lly, 0)
stoptime = time.latch();

runner.assert(err == 0)
runner.assert(tbl ~= nil)

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 1)


starttime = time.latch();
-- tbl, err = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
tbl, err = dem:subset(gm_llx, gm_lly, gm_llx+0.1, gm_lly+0.1)
stoptime = time.latch();

runner.assert(err == 0)
runner.assert(tbl ~= nil)

threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 2)

if tbl ~= nil then
    for i, v in ipairs(tbl) do
        local size = v["size"]
        local mbytes = size / (1024*1024)

        print(string.format("AOI size: %6.1f MB", mbytes))
        runner.assert(size > 0)
    end
end

demType = "landsat-hls"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms))
while not aws.csget("lpdaac-cloud") do
    print("Waiting to authenticate to LPDAAC...")
    sys.wait(1)
end


local geojsonfile = td.."../../plugins/landsat/data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- AOI extent (extent of hls_trimmed.geojson)
llx =  -179.87
lly =    50.45
urx =  -178.27
ury =    51.44


-- dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"B05"}, catalog = contents }))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDVI"}, catalog = contents }))
starttime = time.latch();
tbl, err = dem:sample(-179, 51, 0)
stoptime = time.latch();

runner.assert(err == 0)
runner.assert(tbl ~= nil)

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 9)

starttime = time.latch();
tbl, err = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();

runner.assert(err == 0)
runner.assert(tbl ~= nil)

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 59)

if tbl ~= nil then
    for i, v in ipairs(tbl) do
        local size = v["size"]
        local mbytes = size / (1024*1024)
        runner.assert(size > 0)

        -- This results in 59 threads, all the same size, cols, buffs data type. Print only first one
        if i == 1 then
            print(string.format("AOI size: %6.1f MB", mbytes))
        end

        runner.assert(size > 0)
    end
end

local demType = "esa-worldcover-10meter"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
local lon =    -108.1
local lat =      39.1
local height =    0.0
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)

runner.assert(err == 0)
runner.assert(tbl ~= nil)

local stoptime = time.latch();
threadCnt = 0
if err ~= 0 then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 1)

starttime = time.latch();
tbl, err = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
stoptime = time.latch();

runner.assert(err == 0)
runner.assert(tbl ~= nil)

local threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.assert(threadCnt == 1)

for i, v in ipairs(tbl) do
    local size = v["size"]
    local mbytes = size / (1024*1024)

    print(string.format("AOI size: %6.1f MB", mbytes))
    runner.assert(size > 0)
end


local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )

