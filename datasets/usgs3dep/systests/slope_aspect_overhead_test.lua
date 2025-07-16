local runner = require("test_executive")

-- Setup ---------------------------------------------------------------
local _, td = runner.srcscript()

--  catalog polygon
local geojsonfile = td.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile,"r");
local contents = f:read("*all"); f:close()

--  POI list
local poifile = td.."../../landsat/data/grand_mesa_poi.txt"
local lons, lats, heights = {}, {}, {}
for line in io.lines(poifile) do
    local lon, lat = line:match("(%S+)%s+(%S+)")
    if lon and lat then
        table.insert(lons, tonumber(lon))
        table.insert(lats, tonumber(lat))
        table.insert(heights, 0)          -- constant height
    end
end

----------------------------------------------------------------------
-- helper to run one batch pass and return exec‑time & failures
----------------------------------------------------------------------
local function run_batch(dem, lons, lats, heights)
    local t0 = time.latch()
    local tbl, err = dem:batchsample(lons, lats, heights)
    runner.assert(err == 0, "batchsample error")

    local failures = 0
    for _, pointSamples in ipairs(tbl) do
        if #pointSamples == 0 then failures = failures + 1 end
    end
    local dt = time.latch() - t0
    return dt, failures
end


-- limit to first N points for this test
local numPoints = 20000
lons     = {table.unpack(lons,     1, numPoints)}
lats     = {table.unpack(lats,     1, numPoints)}
heights  = {table.unpack(heights,  1, numPoints)}

local loops = 3  -- how many times to repeat each test


-- Raster
local demType = "usgs3dep-10meter-dem"

----------------------------------------------------------------------
-- 1) slope_aspect enabled
----------------------------------------------------------------------
print(string.format(
    "\n--------------------------------------------------------------------------\n"..
    "%s test with slope aspect (%d pts) %d loops\n"..
    "--------------------------------------------------------------------------", demType, numPoints, loops))
local time_sa, fail_sa
for pass = 1, loops do
    print(string.format("Pass %d/%d", pass, loops))
    local dem_sa = geo.raster(geo.parms{
        asset              = demType,
        catalog            = contents,
        slope_aspect       = true,
        slope_scale_length = 40
    })
    time_sa, fail_sa = run_batch(dem_sa, lons, lats, heights)
end
print(string.format(
    "After %d runs WITH slope/aspect: last exec‑time = %.3f s, failures = %d",
    loops, time_sa, fail_sa))

----------------------------------------------------------------------
-- 2) slope_aspect disabled
----------------------------------------------------------------------
print(string.format(
    "\n--------------------------------------------------------------------------\n"..
    "%s test without slope aspect (%d pts) %d loops\n"..
    "--------------------------------------------------------------------------", demType, numPoints, loops))
local time_no, fail_no
for pass = 1, loops do
    print(string.format("Pass %d/%d", pass, loops))
    local dem_no = geo.raster(geo.parms{
        asset   = demType,
        catalog = contents               -- no slope_aspect key
    })
    time_no, fail_no = run_batch(dem_no, lons, lats, heights)
end
print(string.format(
    "After %d runs WITHOUT slope/aspect: last exec‑time = %.3f s, failures = %d",
    loops, time_no, fail_no))

----------------------------------------------------------------------
-- comparison
----------------------------------------------------------------------
if time_no > 0 then
    local ratio = time_sa / time_no
    print(string.format("Slope/aspect adds a factor of %.2fx", ratio))
end


-- Repeat for 1 meter

local demType = "usgs3dep-1meter-dem"

----------------------------------------------------------------------
-- 1) slope_aspect enabled
----------------------------------------------------------------------
print(string.format(
    "\n--------------------------------------------------------------------------\n"..
    "%s test with slope aspect (%d pts) %d loops\n"..
    "--------------------------------------------------------------------------", demType, numPoints, loops))
local time_sa, fail_sa
for pass = 1, loops do
    print(string.format("Pass %d/%d", pass, loops))
    local dem_sa = geo.raster(geo.parms{
        asset              = demType,
        catalog            = contents,
        slope_aspect       = true,
        slope_scale_length = 40
    })
    time_sa, fail_sa = run_batch(dem_sa, lons, lats, heights)
end
print(string.format(
    "After %d runs WITH slope/aspect: last exec‑time = %.3f s, failures = %d",
    loops, time_sa, fail_sa))


----------------------------------------------------------------------
-- 2) slope_aspect disabled
----------------------------------------------------------------------
print(string.format(
    "\n--------------------------------------------------------------------------\n"..
    "%s test without slope aspect (%d pts) %d loops\n"..
    "--------------------------------------------------------------------------", demType, numPoints, loops))
local time_no, fail_no
for pass = 1, loops do
    print(string.format("Pass %d/%d", pass, loops))
    local dem_no = geo.raster(geo.parms{
        asset   = demType,
        catalog = contents               -- no slope_aspect key
    })
    time_no, fail_no = run_batch(dem_no, lons, lats, heights)
end
print(string.format(
    "After %d runs WITHOUT slope/aspect: last exec‑time = %.3f s, failures = %d",
    loops, time_no, fail_no))

----------------------------------------------------------------------
-- comparison
----------------------------------------------------------------------
if time_no > 0 then
    local ratio = time_sa / time_no
    print(string.format("Slope/aspect adds a factor of %.2fx", ratio))
end

sys.quit(0)
