--
-- ENDPOINT:    /source/atl24g
--

local endpoint_start_time = time.gps()

local json          = require("json")
local georesource   = require("georesource")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")
local prettyprint   = require("prettyprint")

-- get inputs 
local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]
local timeout       = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT

-- create user log publisher (alerts)
local userlog = msg.publish(rspq)

-- initialize timing profiling table
local profile = {}

-- acquire lock for processing granule
local transaction_id = netsvc.INVALID_TX_ID
while transaction_id == netsvc.INVALID_TX_ID do
    transaction_id = netsvc.orchselflock("sliderule", timeout, 3)
    if transaction_id == netsvc.INVALID_TX_ID then
        local lock_retry_wait_time = (time.gps() - endpoint_start_time) / 1000.0
        if lock_retry_wait_time >= timeout then
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> failed to acquire lock... exiting", rspq))
            return
        else
            userlog:alert(core.INFO, core.RTE_TIMEOUT, string.format("request <%s> failed to acquire lock... retrying with %f seconds left", rspq, timeout - lock_retry_wait_time))
            sys.wait(30) -- seconds
        end
    end
end

-- populate resource via CMR request (ONLY IF NOT SUPPLIED)
if not resource then
    local atl03_cmr_start_time = time.gps()
    local rc, rsps = earthdata.cmr(parms)
    if rc == earthdata.SUCCESS then
        local resources = rsps
        if #resources ~= 1 then
            userlog:alert(core.INFO, core.RTE_ERROR, string.format("request <%s> failed to retrieved single resource from CMR <%d>", rspq, #resources))
            return
        end
    else
        userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("request <%s> failed to make CMR request <%d>: %s", rspq, rc, rsps))
        return
    end
    profile["atl03_cmr"] = (time.gps() - atl03_cmr_start_time) / 1000.0
    userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL03 CMR search executed in %f seconds", profile["atl03_cmr"]))
end

-- intialize processing environment
local args = {
    shard           = rqst["shard"] or 0, -- key space
    default_asset   = "icesat2",
    result_q        = parms[geo.PARMS] and "result." .. resource .. "." .. rspq or rspq,
    result_rec      = "bathyrec",
    userlog         = userlog
}
local proc = georesource.initialize(resource, parms, nil, args)
if not proc then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("failed to initialize processing of %s", resource))
    return
end

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

-- build hls parameters
local year      = resource:sub(7,10)
local month     = resource:sub(11,12)
local day       = resource:sub(13,14)
local rdate     = string.format("%04d-%02d-%02dT00:00:00Z", year, month, day)
local rgps      = time.gmt2gps(rdate)
local rdelta    = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
local t0        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
local t1        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))

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
local geo_parms = nil
local rc1, rsps1 = earthdata.stac(hls_parms)
if rc1 == earthdata.SUCCESS then
    hls_parms["catalog"] = json.encode(rsps1)
    geo_parms = geo.parms(hls_parms)
end
profile["hls_stac"] = (time.gps() - stac_start_time) / 1000.0
userlog:alert(core.INFO, core.RTE_INFO, string.format("HLS STAC search executed in %f seconds", profile["hls_stac"]))

-- get ATL09 resources
local atl09_cmr_start_time = time.gps()
local original_asset = parms["asset"]
local original_t0 = parms["t0"]
local original_t1 = parms["t1"]
parms["asset"] = "icesat2-atl09"
parms["t0"] = t0
parms["t1"] = t1
local rc2, rsps2 = earthdata.search(parms)
if rc2 == earthdata.SUCCESS then
    parms["resources09"] = rsps2
else
    prettyprint.display(parms)
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to make ATL09 CMR request <%d>: %s", rspq, rc2, rsps2))
    return
end
parms["asset"] = original_asset
parms["t0"] = original_t0
parms["t1"] = original_t1
profile["atl09_cmr"] = (time.gps() - atl09_cmr_start_time) / 1000.0
userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL09 CMR search executed in %f seconds", profile["atl09_cmr"]))

-- get ATL09 asset
local atl09_asset_name = parms["atl09_asset"] or parms["asset"] or args.default_asset
local atl09_asset = core.getbyname(atl09_asset_name)
if not atl09_asset then
    userlog:alert(core.INFO, core.RTE_ERROR, string.format("invalid asset specified for ATL09: %s", atl09_asset_name))
    return
end

-- initialize container runtime environment
local crenv = runner.setup()
if not crenv.host_sandbox_directory then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("failed to initialize container runtime for %s", resource))
    return
end

-- read ICESat-2 inputs
local bathy_parms   = icesat2.bathyparms(parms)
local reader        = icesat2.atl03bathy(proc.asset, resource, args.result_q, bathy_parms, geo_parms, crenv.host_sandbox_directory, false, atl09_asset)
local status        = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, false)
if not status then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("failed to generate ATL03 bathy inputs for %s", resource))
    runner.cleanup(crenv)
end

-- capture setup time
profile["total_input_generation"] = (time.gps() - endpoint_start_time) / 1000.0
userlog:alert(core.INFO, core.RTE_INFO, string.format("sliderule setup executed in %f seconds", profile["total_input_generation"]))

-- free reader to save on memory usage (safe since all data is now written out)
reader:destroy()

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
local output_files  = {}
output_files["spot_photons"] = {}
output_files["spot_granule"] = {}
output_files["classifiers"] = {}

-- function: run classifier
local function runclassifier(output_table, _bathy_parms, container_timeout, name, in_parallel, command_override)
    if not _bathy_parms:classifieron(name) then
        return
    end
    local start_time = time.gps()
    output_table["classifiers"][name] = {}
    local container_list = {}
    for i = 1,icesat2.NUM_SPOTS do
        if _bathy_parms:spoton(i) then
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

-- function: run processor (overwrites input csv file)
local function runprocessor(_bathy_parms, container_timeout, name, in_parallel, command_override)
    local start_time = time.gps()
    local container_list = {}
    for i = 1,icesat2.NUM_SPOTS do
        if _bathy_parms:spoton(i) then
            local settings_filename = string.format("%s/%s.json", crenv.container_sandbox_mount, name)                           -- e.g. /share/openoceans.json
            local parameters_filename = string.format("%s/%s_%d.json", crenv.container_sandbox_mount, icesat2.BATHY_PREFIX, i)   -- e.g. /share/bathy_spot_3.json
            local input_filename = string.format("%s/%s_%d.csv", crenv.container_sandbox_mount, icesat2.BATHY_PREFIX, i)         -- e.g. /share/bathy_spot_3.csv
            local output_filename = input_filename -- overwrite input with new values
            local container_command = command_override or string.format("/env/bin/python /%s/runner.py", name)
            local container_parms = {
                image =  "oceaneyes",
                name = name,
                command = string.format("%s %s %s %s %s", container_command, settings_filename, parameters_filename, input_filename, output_filename),
                timeout = container_timeout,
                parms = { [name..".json"] = parms[name] or {void=true} }
            }
            local container = runner.execute(crenv, container_parms, rspq)
            table.insert(container_list, container)
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

-- determine parallelism
local in_parallel = true

-- execute qtrees surface finding algorithm
runclassifier(output_files, bathy_parms, timeout, "qtrees", in_parallel, "bash /qtrees/runner.sh")

-- perform refraction correction
runprocessor(bathy_parms, timeout, "atl24refraction", in_parallel)

-- perform uncertainty calculations
runprocessor(bathy_parms, timeout, "atl24uncertainty", in_parallel)

-- execute medianfilter bathy
runclassifier(output_files, bathy_parms, timeout, "medianfilter", in_parallel)

-- execute cshelph bathy
runclassifier(output_files, bathy_parms, timeout, "cshelph", in_parallel)

-- execute bathypathfinder bathy
runclassifier(output_files, bathy_parms, timeout, "bathypathfinder", in_parallel)

-- execute coastnet bathy
runclassifier(output_files, bathy_parms, timeout, "coastnet", false, "bash /coastnet/runner.sh")

-- execute pointnet2 bathy
runclassifier(output_files, bathy_parms, timeout, "pointnet2", false)

-- execute openoceans
runclassifier(output_files, bathy_parms, timeout, "openoceans", false)


-- capture endpoint timing
profile["atl24_endpoint"] = (time.gps() - endpoint_start_time) / 1000.0
userlog:alert(core.INFO, core.RTE_INFO, string.format("atl24 endpoint executed in %f seconds", profile["atl24_endpoint"]))

-- build final output
local output_parms = parms[arrow.PARMS] or {
    path = string.gsub(resource, "ATL03", "ATL24"),
    format = "hdf5",
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
            profile = profile
        }
    }
}
local container = runner.execute(crenv, writer_parms, rspq)
runner.wait(container, timeout)

-- send final output to user
arrow.send2user(crenv.host_sandbox_directory.."/atl24.bin", arrow.parms(output_parms), rspq)

-- send metadata output to user
output_parms["path"] = output_parms["path"]..".json"
arrow.send2user(crenv.host_sandbox_directory.."/atl24.bin.json", arrow.parms(output_parms), rspq)

-- cleanup container runtime environment
--runner.cleanup(crenv)

-- unlock transaction
netsvc.orchunlock({transaction_id})

