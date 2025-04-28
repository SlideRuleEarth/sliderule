local earthdata = require("earth_data_query")

-----------------
--- Get ATL09
-----------------
local function get_atl09(parms, t0, t1, userlog, resource)
    local resource09 = nil
    local atl09_parms = {
        asset = "icesat2-atl09",
        t0 = t0,
        t1 = t1,
        name_filter = '*_' .. string.format("%04d", parms["rgt"]) .. '????_*'
    }
    local atl09_max_retries = 3
    local atl09_attempt = 1
    while true do
        local rc2, rsps2 = earthdata.search(atl09_parms)
        if rc2 == earthdata.SUCCESS then
            if #rsps2 == 1 then
                resource09 = rsps2[1]
                break -- success
            else
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("returned an invalid number of resources for ATL09 CMR request for %s: %d", resource, #rsps2))
                break -- failure
            end
        else
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("failed attempt %d to make ATL09 CMR request <%d>: %s", atl09_attempt, rc2, rsps2))
            atl09_attempt = atl09_attempt + 1
            if atl09_attempt > atl09_max_retries then
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("failed to make ATL09 CMR request for %s... aborting!", resource))
                break -- failure
            end
        end
    end
    return h5.object(parms["asset09"], resource09)
end

-----------------
--- Get NDWI
-----------------
local function get_ndwi(parms, resource)
    local geo_parms = nil
    -- build hls polygon
    local hls_poly = parms["poly"]
    if not hls_poly then
        local original_name_filter = parms["name_filter"]
        parms["name_filter"] = "*" .. resource
        local rc, rsps = earthdata.cmr(parms, nil, true)
        if rc == earthdata.SUCCESS then
            hls_poly = rsps[resource] and rsps[resource]["poly"]
        end
        parms["name_filter"] = original_name_filter
    end
    -- build hls raster object
    local hls_parms = {
        asset       = "landsat-hls",
        t0          = t0,
        t1          = t1,
        use_poi_time = true,
        bands       = {"NDWI"},
        poly        = hls_poly
    }
    local rc1, rsps1 = earthdata.stac(hls_parms)
    if rc1 == earthdata.SUCCESS then
        hls_parms["catalog"] = json.encode(rsps1)
        geo_parms = geo.parms(hls_parms)
    end
    -- return
    return geo_parms
end

-----------------
--- Get VIIRS
-----------------
local function get_viirs(parms, rgps)
    local _,doy         = time.gps2gmt(rgps)
    local doy_8d_start  = ((doy - 1) & ~7) + 1
    local doy_8d_stop   = doy_8d_start + 7
    local gps_start     = time.gmt2gps(string.format("%04d:%03d:00:00:00", parms["year"], doy_8d_start))
    local gps_stop      = time.gmt2gps(string.format("%04d:%03d:00:00:00", parms["year"], doy_8d_stop))
    local year_start, month_start, day_start = time.gps2date(gps_start)
    local year_stop,  month_stop,  day_stop  = time.gps2date(gps_stop)
    if year_start ~= year_stop then
        year_stop = year_start
        month_stop = 12
        day_stop = 31
    end
    local viirs_filename = string.format("JPSS1_VIIRS.%04d%02d%02d_%04d%02d%02d.L3m.8D.KD.Kd_490.4km.nc.dap.nc4",
        year_start, month_start, day_start,
        year_stop, month_stop, day_stop
    )
    return bathy.kd(parms, viirs_filename)
end

-----------------
--- Package
-----------------

local package = {
    get_atl09 = get_atl09,
    get_ndwi = get_ndwi,
    get_viirs = get_viirs
}

return package