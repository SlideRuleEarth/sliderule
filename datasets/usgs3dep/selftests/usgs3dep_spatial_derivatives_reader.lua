local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
runner.log(core.DEBUG)

local geojsonfile = dirpath.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Self Test --

local  sigma = 1.0e-4

local lons, lats, heights = {}, {}, {}

lons = { -108.1}
lats = {   39.1}
heights = { 0 }

local scales = {0, 40, 100}          -- the slope_scale_length values to cycle through
local demType = "usgs3dep-10meter-dem"

local expResults = {{2638.235986840923, 1354129161.0, '/vsis3/prd-tnm/StagedProducts/Elevation/13/TIFF/USGS_Seamless_DEM_13.vrt'}}

for _, scale in ipairs(scales) do
    print(string.format(
        "\n------------------------------\nusgs3dep 10meter slope_scale_length = %d\n------------------------------",
        scale))

    -- (re)create raster with current scale
    local dem = geo.raster(geo.parms{
        asset            = demType,
        slope_aspect     = true,
        slope_scale_length = scale
    })

    local tbl, err = dem:sample(lons[1], lats[1], heights[1])
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)

    local sampleCnt = 0
    for i, v in ipairs(tbl) do
        local el     = v["value"]
        local fname  = v["file"]
        local time   = v["time"]
        local slope  = v["slope"]
        local aspect = v["aspect"]
        local count  = v["count"]

        print(string.format(
            "(%02d) %17.12f  time: %.2f  slope: %f  aspect: %f  count: %d  %s",
            i, el, time, slope, aspect, count, fname))

        sampleCnt = sampleCnt + 1

        if tostring(el) ~= "nan" then
            runner.assert(math.abs(el - expResults[i][1]) < sigma)
        end
        runner.assert(time == expResults[i][2])
        runner.assert(fname == expResults[i][3])
    end
    runner.assert(sampleCnt == #expResults)
end


print(string.format("\n-------------------------------------------------\nusgs3dep 1meter DEM slope, aspect sample\n-------------------------------------------------"))


local expResults = {{2638.032147717071, 1289671725.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM12_2016.tif'},
                    {math.nan,          1289671840.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM13_2015/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM13_2015.tif'},
                    {2638.115155529571, 1289671830.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x75y434_CO_MesaCo_QL2_UTM12_2016.tif'},
                    {math.nan,          1289671872.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM13_2015/TIFF/USGS_one_meter_x75y434_CO_MesaCo_QL2_UTM13_2015.tif'}}

local demType = "usgs3dep-1meter-dem"
local dem = geo.raster(geo.parms({ asset = demType, catalog = contents, slope_aspect=true, slope_scale_length=0, sort_by_index = true }))
local tbl, err = dem:sample(lons[1], lats[1], heights[1])
runner.assert(err == 0)
runner.assert(tbl ~= nil)

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    local slope = v["slope"]
    local aspect = v["aspect"]
    local count = v["count"]
    print(string.format("(%02d) %17.12f  time: %.2f slope: %f aspect: %f count: %d  %s", i, el, time, slope, aspect, count, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults)


-- Report Results --

runner.report()
