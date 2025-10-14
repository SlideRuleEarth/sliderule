local runner = require("test_executive")

-- Setup ---------------------------------------------------------------
local _, td = runner.srcscript()

runner.log(core.ERROR)

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
-- helper to run one batch pass and return exec‑time & total results
----------------------------------------------------------------------
local function run_batch(dem, lons, lats, heights)
    local t0 = time.latch()
    local tbl, err = dem:batchsample(lons, lats, heights)
    runner.assert(err == 0, "batchsample error")

    local validValues = 0
    local nanValues = 0

    local shownPOI = 0
    for i, pointSamples in ipairs(tbl) do
        for j, s in ipairs(pointSamples) do
            local val = s["value"]
            if val ~= val then  -- NaN check
                nanValues = nanValues + 1
            else
                validValues = validValues + 1
            end

            -- Print first 10 results (any kind)
            -- if shownPOI < 10 then
            --     local text
            --     if val ~= val then
            --         text = string.format("   [%02d] lon=%.6f lat=%.6f value=NaN  file=%s",
            --             i, lons[i], lats[i], s["file"] or "nil")
            --     else
            --         text = string.format("   [%02d] lon=%.6f lat=%.6f value=%10.3f  file=%s",
            --             i, lons[i], lats[i], val, s["file"] or "nil")
            --     end
            --     print(text)
            --     shownPOI = shownPOI + 1
            -- end
        end
    end

    local totalResults = validValues + nanValues
    local dt = time.latch() - t0

    print(string.format("Valid numeric values: %d", validValues))
    print(string.format("NaN values: %d", nanValues))
    print(string.format("Total returned: %d (check: %s)",
        totalResults,
        (totalResults == validValues + nanValues) and "OK" or "MISMATCH"))

    return dt, totalResults
end



-- limit to first N points for this test
local numPoints = 20000
lons     = {table.unpack(lons,     1, numPoints)}
lats     = {table.unpack(lats,     1, numPoints)}
heights  = {table.unpack(heights,  1, numPoints)}

local loops = 3  -- how many times to repeat each test


-- Raster
local demType = "usgs3dep-1meter-dem"
print(string.format(
    "\n--------------------------------------------------------------------------\n"..
    "%s test 1 meter index raster method (%d pts) %d loops\n"..
    "--------------------------------------------------------------------------", demType, numPoints, loops))
local time_no, results
for pass = 1, loops do
    print(string.format("Pass %d/%d", pass, loops))
    local dem_no = geo.raster(geo.parms{
        asset   = demType,
        catalog = contents               -- no slope_aspect key
    })
    time_no, results = run_batch(dem_no, lons, lats, heights)
end
print(string.format(
    "After %d runs 1m regular: last exec‑time = %.3f s, all results = %d",
    loops, time_no, results))

-- Repeat for 1 meter GTI

local demType = "usgs3dep-1meter-gti-dem"
print(string.format(
    "\n--------------------------------------------------------------------------\n"..
    "%s test 1 meter GTI driver (%d pts) %d loops\n"..
    "--------------------------------------------------------------------------", demType, numPoints, loops))
local time_sa, results_gti
for pass = 1, loops do
    print(string.format("Pass %d/%d", pass, loops))
    local dem_sa = geo.raster(geo.parms{
        asset              = demType,
        catalog            = contents
    })
    time_sa, results_gti= run_batch(dem_sa, lons, lats, heights)
end
print(string.format(
    "After %d runs 1m GTI: last exec‑time = %.3f s, all results = %d",
    loops, time_sa, results_gti))


sys.quit(0)
