local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local csv = require("csv")
local json = require("json")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

local _,td = runner.srcscript()
local geojsonfile = td.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  sigma = 1.0e-4
local  lon =    -108.1
local  lat =      39.1
local  height = 2630.0


-- NOTE: to validate that the custom transform from SLIDERULE EPSG to 1m 3DEP raster pixels worked use the following command:
-- echo -108.1 39.1 2630.0 | PROJ_DEBUG=2 PROJ_NETWORK=ON cs2cs -r -s epsg:7912 epsg:6342+5703
-- at the end of output there should be a statement:
-- Using https://cdn.proj.org/us_noaa_g2018u0.tif
-- 4332449.53      231917.92  2646.53
--
-- Initial Z0 value is 2630.0 after transform it is Z1 2646.53
-- So Geoid shift is (Z0-Z1) = -16.526
-- Value at pixel in the raster must be adjusted by
-- RETURN_VAL = 2654.55810546875 + -16.526 =  2638.03214


print(string.format("\n-------------------------------------------------\nusgs3dep 1meter DEM sample\n-------------------------------------------------"))


local expResults = {{2638.032147717071, 1289671725.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM12_2016.tif'},
                    {math.nan,          1289671840.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM13_2015/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM13_2015.tif'},
                    {2638.115155529571, 1289671830.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x75y434_CO_MesaCo_QL2_UTM12_2016.tif'},
                    {math.nan,          1289671872.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM13_2015/TIFF/USGS_one_meter_x75y434_CO_MesaCo_QL2_UTM13_2015.tif'}}

local demType = "usgs3dep-1meter-dem"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))
local tbl, err = dem:sample(lon, lat, height)
runner.check(err == 0)
runner.check(tbl ~= nil)

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.check(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.check(time == expResults[i][2])
    runner.check(fname == expResults[i][3])
end
runner.check(sampleCnt == #expResults)


print(string.format("\n-------------------------------------------------\nusgs3dep 1meter DEM subset\n-------------------------------------------------"))

-- AOI extent (extent of grandmesa.geojson)
local gm_llx = -108.3412
local gm_lly =   38.8236
local gm_urx = -107.7292
local gm_ury =   39.1956

local starttime = time.latch();
local tbl, err = dem:subset(gm_llx, gm_lly, gm_llx+0.01, gm_lly+0.4)
local stoptime = time.latch();

runner.check(err == 0)
runner.check(tbl ~= nil)

local threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))
runner.check(threadCnt == 4)

for i, v in ipairs(tbl) do
    local size = v["size"]

    local mbytes = size / (1024*1024)
    -- This results in 4 threads, all the same size, cols, buffs data type. Print only first one
    if i == 1 then
        print(string.format("AOI subset datasize: %.1f MB", mbytes))
    end

    runner.check(size > 0)
end



-- Report Results --

runner.report()

