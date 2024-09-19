--
-- ENDPOINT:    /source/atl24g
--
-------------------------------------------------------
-- table of files being processed
--  {
--      "spot_photons": {
--          [1]: "/share/bathy_spot_1.csv",
--          [2]: "/share/bathy_spot_2.csv",
--          ...
--      }   
--      "spot_granule": {
--          [1]: "/share/bathy_spot_1.json",
--          [2]: "/share/bathy_spot_2.json",
--          ...
--      }
--      "classifiers": {
--          "openoceans": {
--              [1]: "/share/openoceans_1.csv",
--              [2]: "/share/openoceans_2.csv",
--              ...
--          }   
--          "medianfilter": {
--              [1]: "/share/medianfilter_1.csv",
--              [2]: "/share/medianfilter_2.csv",
--              ...
--          }   
--          ...
--          "ensemble": {
--              [1]: "/share/ensemble_1.csv",
--              [2]: "/share/ensemble_2.csv",
--              ...
--          }
--      }
--  }
-------------------------------------------------------

local json          = require("json")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")

local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"])
local resource      = parms["resource"]
local timeout       = parms["node_timeout"]
local rqsttime      = time.gps()
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)
local output_files  = { spot_photons={}, spot_granule={}, classifiers={} }
local profile       = {} -- initialize timing profiling table

-------------------------------------------------------
-- function: get_input_filename
-------------------------------------------------------
local function get_input_filename(_crenv, spot)
    return string.format("%s/%s_%d.csv", _crenv.container_sandbox_mount, bathy.BATHY_PREFIX, spot) -- e.g. /share/bathy_spot_3.csv
end

-------------------------------------------------------
-- function: get_info_filename
-------------------------------------------------------
local function get_info_filename(_crenv, spot)
    return string.format("%s/%s_%d.json", _crenv.container_sandbox_mount, bathy.BATHY_PREFIX, spot)   -- e.g. /share/bathy_spot_3.json
end

-------------------------------------------------------
-- function: cleanup 
-------------------------------------------------------
local function cleanup(_crenv, _transaction_id)
    runner.cleanup(_crenv) -- container runtime environment
    core.orchunlock({_transaction_id}) -- unlock transaction
end

-------------------------------------------------------
-- function: run classifier
-------------------------------------------------------
local function runclassifier(_crenv, output_table, container_timeout, name, in_parallel, command_override)
    local start_time = time.gps()
    output_table["classifiers"][name] = {}
    local container_list = {}
    for i = 1,icesat2.NUM_SPOTS do
        local spot_key = string.format("spot_%d", i)
        local settings_filename = string.format("%s/%s.json", _crenv.container_sandbox_mount, name) -- e.g. /share/openoceans.json
        local parameters_filename = get_info_filename(_crenv, i)
        local input_filename = get_input_filename(_crenv, i)
        local output_filename = string.format("%s/%s_%d.csv", _crenv.container_sandbox_mount, name, i) -- e.g. /share/openoceans_3.csv
        local container_command = command_override or string.format("/env/bin/python /%s/runner.py", name)
        local container_parms = {
            image =  "oceaneyes",
            name = name.."_"..spot_key,
            command = string.format("%s %s %s %s %s", container_command, settings_filename, parameters_filename, input_filename, output_filename),
            timeout = container_timeout,
            parms = { [name..".json"] = parms[name] or {void=true} }
        }
        local container = runner.execute(_crenv, container_parms, rspq)
        table.insert(container_list, container)
        output_table["classifiers"][name][spot_key] = output_filename
        if not in_parallel then
            runner.wait(container, container_timeout)
        end
    end
    if in_parallel then
        for _,container in ipairs(container_list) do
            runner.wait(container, container_timeout)
        end
    end
    local stop_time = time.gps()
    local duration = (stop_time - start_time) / 1000.0
    userlog:alert(core.INFO, core.RTE_INFO, string.format("%s executed in %f seconds", name, duration))
    return duration
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
-- get geoparms for NDWI (if requested)
-------------------------------------------------------
local geo_parms = nil
if parms["generate_ndwi"] then

    -- build time range
    local rdelta = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
    local t0 = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
    local t1 = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))

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
    t0 = parms["t0"],
    t1 = parms["t1"],
    name_filter = '*_'..resource:sub(22,25)..'????_*' -- rgt
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
-- build dataframes for each beam
-------------------------------------------------------

-- create dataframe inputs
local granule = (parms["output"]["format"] == "h5") and bathy.granule(parms, rspq) or nil
local bathymask = bathy.mask()
local atl03h5 = h5.object(parms["asset"], resource)
local atl09h5 = h5.object(parms["asset09"], resource09)
local kd490 = bathy.kd(parms, viirs_filename)
local seasurface = bathy.seasurface(parms)
local refraction = bathy.refraction(parms)
local uncertainty = bathy.uncertainty(parms, kd490)
local qtrees = qtrees.classifier(qtrees.parms(parms["qtrees"]))
local coastnet = coastnet.classifier(coastnet.parms(parms["coastnet"]))
local openoceanspp = openoceanspp.classifier(openoceanspp.parms(parms["openoceanspp"]))

-- create dataframe and run pipeline
local dataframes = {}
for index, beam in ipairs({"gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"}) do
    dataframes[beam] = bathy.dataframe(beam, parms, bathymask, atl03h5, atl09h5, rspq)
    if not dataframes[beam] then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on resource %s failed to create bathy dataframe for beam %s", rspq, resource, beam))
    elseif not dataframes[beam]:isvalid() then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on resource %s failed to create valid bathy dataframe for beam %s", rspq, resource, beam))
    elseif dataframes[beam]:length() <= 0 then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on resource %s created an empty bathy dataframe for beam %s", rspq, resource, beam))
    else
        dataframes[beam].run(seasurface)
        dataframes[beam].run(refraction)
        dataframes[beam].run(uncertainty)
        dataframes[beam].run(qtrees)
        dataframes[beam].run(coastnet)
        dataframes[beam].run(openoceanspp)
        dataframes[beam].run(nil)
    end
end

-- wait for dataframes to complete
if granule then granule:waiton(timeout * 1000, rspq) end
for beam,dataframe in pairs(dataframes) do
    dataframe.waiton(timeout * 1000, rspq)
    local spot_file = arrow.dataframe(parms, dataframe)
    spot_file:export(string.format("%s/%s_%d.parquet", crenv.container_sandbox_mount, bathy.BATHY_PREFIX, dataframe:meta("spot"), arrow.PARQUET) -- e.g. /share/bathy_spot_3.parquet)
end

-- clean up object to cut down on memory usage
atl03h5:destroy()
atl09h5:destroy()
kd490:destroy()

-- execute python classifiers
local in_parallel = true
runclassifier(crenv, output_files, timeout, "medianfilter", in_parallel)
runclassifier(crenv, output_files, timeout, "cshelph", in_parallel)
runclassifier(crenv, output_files, timeout, "bathypathfinder", in_parallel)
runclassifier(crenv, output_files, timeout, "pointnet", false, "bash /pointnet/runner.sh")
profile["atl24_endpoint"] = (time.gps() - rqsttime) / 1000.0 -- capture endpoint timing
userlog:alert(core.INFO, core.RTE_INFO, string.format("atl24 endpoint executed in %f seconds", profile["atl24_endpoint"]))

local spot_mask = {} -- build spot mask using defaults/parsing from bathyreader (because reader will be destroyed)
for spot = 1,icesat2.NUM_SPOTS do
    spot_mask[spot] = reader:spoton(spot)
    if spot_mask[spot] then
        output_files["spot_photons"][string.format("spot_%d", spot)] = get_input_filename(crenv, spot)
        output_files["spot_granule"][string.format("spot_%d", spot)] = get_info_filename(crenv, spot)
    end
end


profile["reader"] = {
    stats = bathy_stats,
    total_duration = (time.gps() - rqsttime) / 1000.0 -- capture setup time
}
userlog:alert(core.INFO, core.RTE_INFO, string.format("sliderule setup executed in %f seconds", profile["reader"]["total_duration"]))

-------------------------------------------------------
-- build final output
-------------------------------------------------------
local writer_parms = {
    image =  "oceaneyes",
    name = "writer",
    command = string.format("/env/bin/python /atl24writer/runner.py %s/writer_settings.json %s/writer_ancillary.json %s/writer_orbit.json", crenv.container_sandbox_mount, crenv.container_sandbox_mount, crenv.container_sandbox_mount),
    timeout = timeout,
    parms = { 
        ["writer_settings.json"] = {
            input_files = output_files,
            output_parms = output_parms,
            atl24_filename = crenv.container_sandbox_mount.."/atl24.bin",
            profile = profile,
        }
    }
}
local container = runner.execute(crenv, writer_parms, rspq)
runner.wait(container, timeout)

-------------------------------------------------------
-- send final output to user
-------------------------------------------------------
arrow.send2user(crenv.host_sandbox_directory.."/atl24.bin", arrow.parms(output_parms), rspq)
output_parms["path"] = output_parms["path"]..".json"
arrow.send2user(crenv.host_sandbox_directory.."/atl24.bin.json", arrow.parms(output_parms), rspq)

-------------------------------------------------------
-- exit 
-------------------------------------------------------
cleanup(crenv, transaction_id)

