local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

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
local expSlopeResults = {{12.556465, 190.624991,  9},
                         { 8.535686, 169.976891, 49},
                         { 8.944002, 165.335726,169}}

for sindx, scale in ipairs(scales) do
    print(string.format(
        "\n------------------------------\nusgs3dep 10meter slope_scale_length = %d\n------------------------------",
        scale))

    -- (re)create raster with current scale
    local dem = geo.raster(geo.parms{
        asset            = demType,
        slope_aspect     = true,
        slope_scale_length = scale,
        zonal_stats      = true,
        radius           = 100
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
        local stats_count = v["count"]        -- used by zonal stats
        local slope_count = v["slope_count"]  -- used by spatial derivatives

        print(string.format(
            "(%02d) %17.12f  time: %.2f  slope: %f  aspect: %f  slope_count: %d  stats_count: %d  %s",
            i, el, time, slope, aspect, slope_count, stats_count, fname))

        sampleCnt = sampleCnt + 1

        if tostring(el) ~= "nan" then
            runner.assert(math.abs(el - expResults[i][1]) < sigma)
        end
        runner.assert(time == expResults[i][2])
        runner.assert(fname == expResults[i][3])

        runner.assert(math.abs(slope - expSlopeResults[sindx][1]) < sigma)
        runner.assert(math.abs(aspect - expSlopeResults[sindx][2]) < sigma)
        runner.assert(slope_count == expSlopeResults[sindx][3])

        runner.assert(stats_count == 529)  -- from 100 meter radius
    end
    runner.assert(sampleCnt == #expResults)
end

print(string.format("\n-------------------------------------------------\nusgs3dep 1meter DEM slope, aspect sample\n-------------------------------------------------"))


local expResults = {{2638.032147717071, 1289671725.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM12_2016.tif'},
                    {math.nan,          1289671840.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM13_2015/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM13_2015.tif'},
                    {2638.115155529571, 1289671830.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x75y434_CO_MesaCo_QL2_UTM12_2016.tif'},
                    {math.nan,          1289671872.0, '/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM13_2015/TIFF/USGS_one_meter_x75y434_CO_MesaCo_QL2_UTM13_2015.tif'}}

local demType = "usgs3dep-1meter-dem"
local dem = geo.raster(geo.parms({ asset = demType, catalog = contents, slope_aspect=true, slope_scale_length=40, sort_by_index = true }))
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
    local slope_count = v["slope_count"]
    print(string.format("(%02d) %17.12f  time: %.2f slope_count: %d slope: %f aspect: %f %s",
                        i,
                        el or 0.0,
                        time or 0.0,
                        slope_count or 0,
                        slope or 0.0,
                        aspect or 0.0,
                        fname or "nil"))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])

    runner.assert(slope_count == 1681 or slope_count == 0)
end
runner.assert(sampleCnt == #expResults)


-- Report Results --

runner.report()
