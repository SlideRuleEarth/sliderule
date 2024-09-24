--
-- ENDPOINT:    /source/atl24g
--
local json          = require("json")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")

local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"])
local resource      = parms["resource"]
local timeout       = parms["node_timeout"]
local rqsttime      = time.gps()
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)
local outputs       = {} -- table of all outputs that go into atl24 writer
local profile       = {} -- timing profiling table

-------------------------------------------------------
-- function: cleanup 
-------------------------------------------------------
local function cleanup(_crenv, _transaction_id)
    runner.cleanup(_crenv) -- container runtime environment
    core.orchunlock({_transaction_id}) -- unlock transaction
end

-------------------------------------------------------
-- function: ctimeout
-------------------------------------------------------
local function ctimeout()
    local current_timeout = (timeout * 1000) - (time.gps() - rqsttime)
    if current_timeout < 0 then current_timeout = 0 end
    return math.tointeger(current_timeout)
end

-------------------------------------------------------
-- public check
-------------------------------------------------------
if sys.ispublic() then -- verify on a private cluster
    userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> forbidden on public cluster... exiting", rspq))
    return
end

-------------------------------------------------------
-- get Kd resource filename
-------------------------------------------------------
local rdate         = string.format("%04d-%02d-%02dT00:00:00Z", parms["year"], parms["month"], parms["day"])
local rgps          = time.gmt2gps(rdate)
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

-------------------------------------------------------
-- build time range (needed for NDWI and ATL09)
-------------------------------------------------------
local rdelta = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
local t0 = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
local t1 = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))

-------------------------------------------------------
-- get geoparms for NDWI (if requested)
-------------------------------------------------------
local geo_parms = nil
if parms["generate_ndwi"] then

    -- build hls polygon
    local hls_polygon_cmr_start_time = time.gps()
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
    profile["hls_polygon_cmr"] = (time.gps() - hls_polygon_cmr_start_time) / 1000.0
    userlog:alert(core.INFO, core.RTE_INFO, string.format("HLS polygon CMR search executed in %f seconds", profile["hls_polygon_cmr"]))

    -- build hls raster object
    local stac_start_time = time.gps()
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
    profile["hls_stac"] = (time.gps() - stac_start_time) / 1000.0
    userlog:alert(core.INFO, core.RTE_INFO, string.format("HLS STAC search executed in %f seconds", profile["hls_stac"]))

end

-------------------------------------------------------
-- get ATL09 resources
-------------------------------------------------------
local resource09 = nil
local atl09_cmr_start_time = time.gps()
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
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> returned an invalid number of resources for ATL09 CMR request for %s: %d", rspq, resource, #rsps2))
            return -- failure
        end
    else
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed attempt %d to make ATL09 CMR request <%d>: %s", rspq, atl09_attempt, rc2, rsps2))
        atl09_attempt = atl09_attempt + 1
        if atl09_attempt > atl09_max_retries then
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to make ATL09 CMR request for %s... aborting!", rspq, resource))
            return -- failure
        end
    end
end
profile["atl09_cmr"] = (time.gps() - atl09_cmr_start_time) / 1000.0
userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL09 CMR search executed in %f seconds", profile["atl09_cmr"]))

-------------------------------------------------------
-- acquire lock for processing granule (because this is not proxied)
-------------------------------------------------------
local transaction_id = core.INVALID_TX_ID
while transaction_id == core.INVALID_TX_ID do
    transaction_id = core.orchselflock("sliderule", timeout, 3)
    if transaction_id == core.INVALID_TX_ID then
        local lock_retry_wait_time = (time.gps() - rqsttime) / 1000.0
        if lock_retry_wait_time >= timeout then
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> failed to acquire lock... exiting", rspq))
            return -- nothing yet to clean up
        else
            userlog:alert(core.INFO, core.RTE_TIMEOUT, string.format("request <%s> failed to acquire lock... retrying with %f seconds left", rspq, timeout - lock_retry_wait_time))
            sys.wait(30) -- seconds
        end
    end
end

-------------------------------------------------------
-- initialize container runtime environment
-------------------------------------------------------
local crenv = runner.setup()
if not crenv.host_sandbox_directory then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("failed to initialize container runtime for %s", resource))
    cleanup(crenv, transaction_id)
    return
end

-------------------------------------------------------
-- create dataframe inputs
-------------------------------------------------------
local bathymask = bathy.mask()
local atl03h5 = h5.object(parms["asset"], resource)
local atl09h5 = h5.object(parms["asset09"], resource09)
local granule = (parms["output"]["format"] == "h5") and bathy.granule(parms, atl03h5, rspq) or nil
local kd490 = bathy.kd(parms, viirs_filename)
local refraction = bathy.refraction(parms)
local uncertainty = bathy.uncertainty(parms, kd490)
local seasurface = parms["find_sea_surface"] and bathy.seasurface(parms) or nil
local qtrees = parms:classifier(bathy.QTREES) and bathy.qtrees(parms) or nil
local coastnet = parms:classifier(bathy.COASTNET) and bathy.coastnet(parms) or nil
local openoceanspp = parms:classifier(bathy.OPENOCEANSPP) and bathy.openoceanspp(parms) or nil

-------------------------------------------------------
-- build dataframes for each beam
-------------------------------------------------------
local dataframes = {}
for _, beam in ipairs(parms["beams"]) do
    dataframes[beam] = bathy.dataframe(beam, parms, bathymask, atl03h5, atl09h5, rspq)
    if not dataframes[beam] then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create bathy dataframe for beam %s", rspq, resource, beam))
    else
        dataframes[beam]:run(seasurface)
--        dataframes[beam]:run(qtrees)
--        dataframes[beam]:run(coastnet)
--        dataframes[beam]:run(openoceanspp)
--        dataframes[beam]:run(refraction)
--        dataframes[beam]:run(uncertainty)
        dataframes[beam]:run(core.TERMINATE)
    end
end

-------------------------------------------------------
-- wait for dataframes to complete and write to file
-------------------------------------------------------
for beam,dataframe in pairs(dataframes) do
    if dataframe:finished(ctimeout(), rspq) then
        if dataframes[beam]:length() <= 0 then
            userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on %s created an empty bathy dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
        elseif not dataframes[beam]:isvalid() then
            userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on %s failed to create valid bathy dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
            cleanup(crenv, transaction_id)
            return
        else
            outputs[beam] = string.format("%s/bathy_spot_%d.parquet", crenv.host_sandbox_directory, dataframe:meta("spot"))
            local arrow_dataframe = arrow.dataframe(parms, dataframe)
            if not arrow_dataframe then
                userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on %s failed to create arrow dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
                cleanup(crenv, transaction_id)
                return
            elseif not arrow_dataframe:export(outputs[beam], arrow.PARQUET) then
                userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on %s failed to write dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
                cleanup(crenv, transaction_id)
                return
            end
        end
    else
        userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> on %s timed out waiting for dataframe to complete on spot %d", rspq, resource, dataframe:meta("spot")))
        cleanup(crenv, transaction_id)
        return
    end
end

-------------------------------------------------------
-- wait for granule to complete and write to file
-------------------------------------------------------
if granule then
    outputs["granule"] = string.format("%s/bathy_granule.json", crenv.container_sandbox_mount, "w")
    local f = io.open(outputs["granule"])
    if f then
        if granule:waiton(ctimeout(), rspq) then
            f:write(json.encode(granule:export()))
            f:close()
        else
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> timed out waiting for granule to complete on %s", rspq, resource))
            cleanup(crenv, transaction_id)
            return
        end
    else
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to write granule json for %s", rspq, resource))
        cleanup(crenv, transaction_id)
        return
    end
end

-------------------------------------------------------
-- get profiles
-------------------------------------------------------
profile["seasurface"] = seasurface and seasurface:runtime() or 0.0
profile["refraction"] = refraction and refraction:runtime() or 0.0
profile["uncertainty"] = uncertainty and uncertainty:runtime() or 0.0
profile["qtrees"] = qtrees and qtrees:runtime() or 0.0
profile["coastnet"] = coastnet and coastnet:runtime() or 0.0
profile["openoceanspp"] = openoceanspp and openoceanspp:runtime() or 0.0

-- clean up object to cut down on memory usage
atl03h5:destroy()
atl09h5:destroy()
kd490:destroy()

-------------------------------------------------------
-- run oceaneyes
-------------------------------------------------------
local tmp_atl24_filename = "atl24.bin"
local container_parms = {
    image = "oceaneyes",
    name = "oceaneyes",
    command = string.format("/env/bin/python /runner.py %s/settings.json", crenv.container_sandbox_mount),
    timeout = ctimeout(),
    parms = {
        ["settings.json"] = {
            input_files = outputs,
            format = parms["output"]["format"],
            atl24_filename = crenv.container_sandbox_mount.."/"..tmp_atl24_filename,
            profile = profile,
            duration = (time.gps() - rqsttime) / 1000.0
        }
    }
}
local container = runner.execute(crenv, container_parms, rspq)
runner.wait(container, timeout)

-------------------------------------------------------
-- send final output to user
-------------------------------------------------------
arrow.send2user(crenv.host_sandbox_directory.."/"..tmp_atl24_filename, arrow.parms(parms["output"]), rspq)
arrow.send2user(crenv.host_sandbox_directory.."/"..tmp_atl24_filename..".json", arrow.parms(parms["output"]), rspq, parms["output"]["path"]..".json")

-------------------------------------------------------
-- exit 
-------------------------------------------------------
cleanup(crenv, transaction_id)

