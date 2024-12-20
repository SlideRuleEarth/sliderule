--
-- ENDPOINT:    /source/atl24s
--
local json          = require("json")
local dataframe     = require("dataframe")
local earthdata     = require("earthdata")
local runner        = require("container_runtime")
local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"], rqst["shard"], "icesat2", rqst["resource"])
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)
local profile       = {} -- timing profiling table
local outputs       = {} -- table of all outputs that go into oceaneyes
local starttime    = time.gps() -- used for timeout handling
local rdelta        = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
local rdate         = string.format("%04d-%02d-%02dT00:00:00Z", parms["year"], parms["month"], parms["day"])
local rgps          = time.gmt2gps(rdate)
local t0            = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta)) -- start time for NDWI and ATL09
local t1            = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta)) -- stop time for NDWI and ATL09

-------------------------------------------------------
-- Function - acquire lock (only used when not proxied)
-------------------------------------------------------
local function acquireLock(timeout, _starttime)
    local transaction_id = core.INVALID_TX_ID
    while transaction_id == core.INVALID_TX_ID do
        transaction_id = core.orchselflock("sliderule", timeout, 3)
        if transaction_id == core.INVALID_TX_ID then
            local lock_retry_wait_time = (time.gps() - _starttime) / 1000.0
            if lock_retry_wait_time >= timeout then
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> failed to acquire lock... exiting", rspq))
                return -- nothing yet to clean up
            else
                userlog:alert(core.INFO, core.RTE_TIMEOUT, string.format("request <%s> failed to acquire lock... retrying with %f seconds left", rspq, timeout - lock_retry_wait_time))
                sys.wait(30) -- seconds
            end
        end
    end
    return transaction_id
end

-------------------------------------------------------
-- Function - get Kd resource filename
-------------------------------------------------------
local function getKd(_parms)
    local _,doy         = time.gps2gmt(rgps)
    local doy_8d_start  = ((doy - 1) & ~7) + 1
    local doy_8d_stop   = doy_8d_start + 7
    local gps_start     = time.gmt2gps(string.format("%04d:%03d:00:00:00", _parms["year"], doy_8d_start))
    local gps_stop      = time.gmt2gps(string.format("%04d:%03d:00:00:00", _parms["year"], doy_8d_stop))
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
    return viirs_filename, time.gps()
end

-------------------------------------------------------
-- Function - generate parameters for NDWI from HLS
-------------------------------------------------------
local function getNdwiParms(_parms, resource)
    local geo_parms = nil
    -- build hls polygon
    local hls_poly = _parms["poly"]
    if not hls_poly then
        local original_name_filter = _parms["name_filter"]
        _parms["name_filter"] = "*" .. resource
        local rc, rsps = earthdata.cmr(_parms, nil, true)
        if rc == earthdata.SUCCESS then
            hls_poly = rsps[resource] and rsps[resource]["poly"]
        end
        _parms["name_filter"] = original_name_filter
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
    return geo_parms, time.gps()
end

-------------------------------------------------------
-- Function - get ATL09 resources
-------------------------------------------------------
local function getAtl09(resource)
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
                userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> returned an invalid number of resources for ATL09 CMR request for %s: %d", rspq, resource, #rsps2))
                break -- failure
            end
        else
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed attempt %d to make ATL09 CMR request <%d>: %s", rspq, atl09_attempt, rc2, rsps2))
            atl09_attempt = atl09_attempt + 1
            if atl09_attempt > atl09_max_retries then
                userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to make ATL09 CMR request for %s... aborting!", rspq, resource))
                break -- failure
            end
        end
    end
    return resource09, time.gps()
end

-------------------------------------------------------
--Ffunction - cleanup
-------------------------------------------------------
local function cleanup(_crenv, _transaction_id, failure, reason)
    if failure then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed: %s", rspq, reason))
    end
    runner.cleanup(_crenv)
    if _transaction_id then
        core.orchunlock({_transaction_id})
    end
end

-------------------------------------------------------
-- Function - ctimeout
-------------------------------------------------------
local function ctimeout(timeout, _starttime)
    local current_timeout = (timeout * 1000) - (time.gps() - _starttime)
    if current_timeout < 0 then current_timeout = 0 end
    return math.tointeger(current_timeout)
end

-------------------------------------------------------
-- Proxy/Execute/Send
-------------------------------------------------------

-- query earthdata for resources to process
local earthdata_status = earthdata.query(parms, rspq, userlog)
if earthdata_status ~= earthdata.SUCCESS then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> earthdata queury failed : %d", rspq, earthdata_status))
    return
end

-- proxy request
if not parms["resoure"] then
    local df = dataframe.proxy("atl24p", parms, rspq, userlog)
    dataframe.send(df, parms, rspq, userlog)
    return
end

-- get initial resources
local transaction_id = nil
local resource = parms["resoure"]
local viirs_filename = getKd(parms)
local resource09 = getAtl09(resource)

-- acquire lock when not proxied
if parms["shard"] == core.INVALID_KEY then
    transaction_id = acquireLock(parms["timeout"], starttime)
end

-- create container runtime environment
local crenv = runner.setup()

-- create dataframe inputs
local bathymask = bathy.mask()
local atl03h5 = h5.object(parms["asset"], resource)
local atl09h5 = h5.object(parms["asset09"], resource09)
local kd490 = bathy.kd(parms, viirs_filename)
local refraction = bathy.refraction(parms)
local uncertainty = bathy.uncertainty(parms, kd490)
local seasurface = parms["find_sea_surface"] and bathy.seasurface(parms) or nil
local qtrees = parms:classifier(bathy.QTREES) and bathy.qtrees(parms) or nil
local coastnet = parms:classifier(bathy.COASTNET) and bathy.coastnet(parms) or nil
local openoceanspp = parms:classifier(bathy.OPENOCEANSPP) and bathy.openoceanspp(parms) or nil
userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> creating dataframes...", rspq))

-- build dataframes for each beam
local dataframes = {}
for _, beam in ipairs(parms["beams"]) do
    dataframes[beam] = bathy.dataframe(beam, parms, bathymask, atl03h5, atl09h5, rspq)
    if not dataframes[beam] then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create bathy dataframe for beam %s", rspq, resource, beam))
    else
        dataframes[beam]:run(seasurface)
        dataframes[beam]:run(qtrees)
        dataframes[beam]:run(coastnet)
        dataframes[beam]:run(openoceanspp)
        dataframes[beam]:run(refraction)
        dataframes[beam]:run(uncertainty)
        dataframes[beam]:run(core.TERMINATE)
    end
end

-- wait for dataframes to complete and write to file
for beam,beam_df in pairs(dataframes) do
    local df_finished = beam_df:finished(ctimeout(), rspq)
    if not df_finished then
        userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> on %s timed out waiting for dataframe to complete on spot %d", rspq, resource, beam_df:meta("spot")))
    elseif not dataframes[beam]:isvalid() then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on %s failed to create valid bathy dataframe for spot %d", rspq, resource, beam_df:meta("spot")))
    elseif dataframes[beam]:length() == 0 then
        userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> on %s created an empty bathy dataframe for spot %d", rspq, resource, beam_df:meta("spot")))
    else
        local spot = beam_df:meta("spot")
        local arrow_df = arrow.dataframe(parms, beam_df)
        local output_filename = string.format("%s/bathy_spot_%d.parquet", crenv.host_sandbox_directory, spot)
        arrow_df:export(output_filename, arrow.PARQUET)
        userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> on %s created dataframe for spot %s", rspq, resource, spot))
        outputs[beam] = string.format("%s/bathy_spot_%d.parquet", crenv.container_sandbox_mount, spot)
    end
    -- cleanup to save memory
    beam_df:destroy()
end

-- clean up objects to cut down on memory usage
atl03h5:destroy()
if atl09h5 then atl09h5:destroy() end
kd490:destroy()

-- send dataframe back to user
dataframe.send(df, parms, rspq, userlog)

