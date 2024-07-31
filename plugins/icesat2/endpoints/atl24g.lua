--
-- ENDPOINT:    /source/atl24g
--

local json          = require("json")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")
local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]
local timeout       = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT
local interval      = 10 < timeout and 10 or timeout -- seconds

-------------------------------------------------------
-- setup
-------------------------------------------------------
local endpoint_start_time = time.gps()
local profile = {} -- initialize timing profiling table
local userlog = msg.publish(rspq) -- create user log publisher (alerts)
if sys.ispublic() then -- verify on a private cluster
    userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> forbidden on public cluster... exiting", rspq))
    return
end

-------------------------------------------------------
-- function: cleanup 
-------------------------------------------------------
local function cleanup(_crenv, _transaction_id)
    runner.cleanup(_crenv) -- container runtime environment
    netsvc.orchunlock({_transaction_id}) -- unlock transaction
end

-------------------------------------------------------
-- acquire lock for processing granule (because this is not proxied)
-------------------------------------------------------
local transaction_id = netsvc.INVALID_TX_ID
while transaction_id == netsvc.INVALID_TX_ID do
    transaction_id = netsvc.orchselflock("sliderule", timeout, 3)
    if transaction_id == netsvc.INVALID_TX_ID then
        local lock_retry_wait_time = (time.gps() - endpoint_start_time) / 1000.0
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
-- populate resource via CMR request (ONLY IF NOT SUPPLIED)
-------------------------------------------------------
if not resource then
    local atl03_cmr_start_time = time.gps()
    local rc, rsps = earthdata.cmr(parms)
    if rc == earthdata.SUCCESS then
        local resources = rsps
        if #resources ~= 1 then
            userlog:alert(core.INFO, core.RTE_ERROR, string.format("request <%s> failed to retrieved single resource from CMR <%d>", rspq, #resources))
            cleanup(crenv, transaction_id)
            return
        else
            resource = resources[1]
        end
    else
        userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("request <%s> failed to make CMR request <%d>: %s", rspq, rc, rsps))
        cleanup(crenv, transaction_id)
        return
    end
    profile["atl03_cmr"] = (time.gps() - atl03_cmr_start_time) / 1000.0
    userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL03 CMR search executed in %f seconds", profile["atl03_cmr"]))
end

-------------------------------------------------------
-- get time range of data
-------------------------------------------------------
local year      = resource:sub(7,10)
local month     = resource:sub(11,12)
local day       = resource:sub(13,14)
local rdate     = string.format("%04d-%02d-%02dT00:00:00Z", year, month, day)
local rgps      = time.gmt2gps(rdate)
local rdelta    = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
local t0        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
local t1        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))

-------------------------------------------------------
-- get Kd resource filename
-------------------------------------------------------
local _,doy         = time.gps2gmt(rgps)
local doy_8d_start  = ((doy - 1) & ~7) + 1
local doy_8d_stop   = doy_8d_start + 7
local gps_start     = time.gmt2gps(string.format("%04d:%03d:00:00:00", year, doy_8d_start))
local gps_stop      = time.gmt2gps(string.format("%04d:%03d:00:00:00", year, doy_8d_stop))
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
local atl09_cmr_start_time = time.gps()
local rgt = resource:sub(22,25)
local original_asset = parms["asset"]
local original_t0 = parms["t0"]
local original_t1 = parms["t1"]
local original_name_filter = parms["name_filter"]
parms["asset"] = "icesat2-atl09"
parms["t0"] = t0
parms["t1"] = t1
parms["name_filter"] = '*_'..rgt..'????_*'
local rc2, rsps2 = earthdata.search(parms)
if rc2 == earthdata.SUCCESS and #rsps2 == 1 then
    parms["resource09"] = rsps2[1]
else
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to make ATL09 CMR request <%d>: %s", rspq, rc2, rsps2))
    cleanup(crenv, transaction_id)
    return
end
parms["asset"] = original_asset
parms["t0"] = original_t0
parms["t1"] = original_t1
parms["name_filter"] = original_name_filter
profile["atl09_cmr"] = (time.gps() - atl09_cmr_start_time) / 1000.0
userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL09 CMR search executed in %f seconds", profile["atl09_cmr"]))

-------------------------------------------------------
-- build bathy reader parms
-------------------------------------------------------
parms["hls"] = geo_parms
parms["icesat2"] = icesat2.parms(parms)
parms["oceaneyes"] = parms["oceaneyes"] or {}
parms["oceaneyes"]["resource_kd"] = viirs_filename
parms["oceaneyes"]["assetKd"] = parms["oceaneyes"]["assetKd"] or "viirsj1-s3"

-------------------------------------------------------
-- read ICESat-2 inputs
-------------------------------------------------------
local reader = icesat2.bathyreader(parms, resource, rspq, crenv.host_sandbox_directory, false)
if not reader then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to create bathy reader", rspq))
    cleanup(crenv, transaction_id)
    return
end
local spot_mask = {} -- build spot mask using defaults/parsing from bathyreader (because reader will be destroyed)
for spot = 1,icesat2.NUM_SPOTS do
    spot_mask[spot] = reader:spoton(spot)
end
local classifier_mask = {}  -- build classifier mask using defaults/parsing from bathyreader (because reader will be destroyed)
for _,classifier in ipairs({"qtrees", "coastnet", "openoceans", "medianfilter", "cshelph", "bathypathfinder", "pointnet2", "ensemble"}) do
    classifier_mask[classifier] = reader:classifieron(classifier)
end
local duration = 0 -- wait for bathyreader to finish
while (userlog:numsubs() > 0) and not reader:waiton(interval * 1000) do
    duration = duration + interval
    if timeout >= 0 and duration >= timeout then -- check for timeout
        userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration))
        cleanup(crenv, transaction_id)
        return
    end
    userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> ... continuing to read %s (after %d seconds)", rspq, resource, duration))
end
local bathy_stats = reader:stats()
if bathy_stats["photon_count"] <= 0 then
    userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> was not able to read any photons from %s ... aborting!", rspq, resource))
    cleanup(crenv, transaction_id)
    return
end
reader:destroy() -- free reader to save on memory usage (safe since all data is now written out)
profile["total_input_generation"] = (time.gps() - endpoint_start_time) / 1000.0 -- capture setup time
userlog:alert(core.INFO, core.RTE_INFO, string.format("sliderule setup executed in %f seconds", profile["total_input_generation"]))

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
--              [1]: "/share/openoceans_1.json",
--              [2]: "/share/openoceans_2.json",
--              ...
--          }   
--          "medianfilter": {
--              [1]: "/share/medianfilter_1.json",
--              [2]: "/share/medianfilter_2.json",
--              ...
--          }   
--          ...
--          "ensemble": {
--              [1]: "/share/ensemble_1.json",
--              [2]: "/share/ensemble_2.json",
--              ...
--          }
--      }
--  }
-------------------------------------------------------
local output_files  = {}
output_files["spot_photons"] = {}
output_files["spot_granule"] = {}
output_files["classifiers"] = {}

-------------------------------------------------------
-- function: run classifier
-------------------------------------------------------
local function runclassifier(output_table, container_timeout, name, in_parallel, command_override)
    if not classifier_mask[name] then
        return
    end
    local start_time = time.gps()
    output_table["classifiers"][name] = {}
    local container_list = {}
    for i = 1,icesat2.NUM_SPOTS do
        if spot_mask[i] then
            local spot_key = string.format("spot_%d", i)
            local settings_filename = string.format("%s/%s.json", crenv.container_sandbox_mount, name)                           -- e.g. /share/openoceans.json
            local parameters_filename = string.format("%s/%s_%d.json", crenv.container_sandbox_mount, icesat2.BATHY_PREFIX, i)   -- e.g. /share/bathy_spot_3.json
            local input_filename = string.format("%s/%s_%d.csv", crenv.container_sandbox_mount, icesat2.BATHY_PREFIX, i)         -- e.g. /share/bathy_spot_3.csv
            local output_filename = string.format("%s/%s_%d.csv", crenv.container_sandbox_mount, name, i)                        -- e.g. /share/openoceans_3.csv
            local container_command = command_override or string.format("/env/bin/python /%s/runner.py", name)
            local container_parms = {
                image =  "oceaneyes",
                name = name.."_"..spot_key,
                command = string.format("%s %s %s %s %s", container_command, settings_filename, parameters_filename, input_filename, output_filename),
                timeout = container_timeout,
                parms = { [name..".json"] = parms[name] or {void=true} }
            }
            local container = runner.execute(crenv, container_parms, rspq)
            table.insert(container_list, container)
            output_table["spot_photons"][spot_key] = input_filename
            output_table["spot_granule"][spot_key] = parameters_filename
            output_table["classifiers"][name][spot_key] = output_filename
            if not in_parallel then
                runner.wait(container, container_timeout)
            end
        end
    end
    if in_parallel then
        for _,container in ipairs(container_list) do
            runner.wait(container, container_timeout)
        end
    end
    local stop_time = time.gps()
    profile[name] = (stop_time - start_time) / 1000.0
    userlog:alert(core.INFO, core.RTE_INFO, string.format("%s executed in %f seconds", name, profile[name]))
end

-------------------------------------------------------
-- execute classifiers
-------------------------------------------------------
local in_parallel = true
runclassifier(output_files, timeout, "qtrees", in_parallel, "bash /qtrees/runner.sh")
runclassifier(output_files, timeout, "medianfilter", in_parallel)
runclassifier(output_files, timeout, "cshelph", in_parallel)
runclassifier(output_files, timeout, "bathypathfinder", in_parallel)
runclassifier(output_files, timeout, "coastnet", in_parallel, "bash /coastnet/runner.sh")
runclassifier(output_files, timeout, "pointnet2", false, "bash /pointnet2/runner.sh")
runclassifier(output_files, timeout, "openoceans", false)
profile["atl24_endpoint"] = (time.gps() - endpoint_start_time) / 1000.0 -- capture endpoint timing
userlog:alert(core.INFO, core.RTE_INFO, string.format("atl24 endpoint executed in %f seconds", profile["atl24_endpoint"]))

-------------------------------------------------------
-- build final output
-------------------------------------------------------
local version, commit, environment, _launch, _duration, _packages = sys.version()
local output_parms = parms[arrow.PARMS] or {
    path = string.gsub(resource, "ATL03", "ATL24"),
    format = "h5",
    as_geo = false
}
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
            version = version,
            commit = commit,
            environment = environment,
            resource = resource
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

