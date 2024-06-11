--
-- ENDPOINT:    /source/atl24g
--

local endpoint_start_time = time.gps()

local json          = require("json")
local georesource   = require("georesource")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")

-- get inputs 
local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]
local timeout       = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT

-- create user log publisher (alerts)
local userlog = msg.publish(rspq)

-- populate resource via CMR request (ONLY IF NOT SUPPLIED)
if not resource then
    local atl03_cmr_start_time = time.gps()
    local rc, rsps = earthdata.cmr(parms)
    if rc == earthdata.SUCCESS then
        local resources = rsps
        if #resources ~= 1 then
            userlog:alert(core.INFO, core.RTE_ERROR, string.format("proxy request <%s> failed to retrieved single resource from CMR <%d>", rspq, #resources))
            return
        end
    else
        userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("proxy request <%s> failed to make CMR request <%d>: %s", rspq, rc, rsps))
        return
    end
    userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL03 CMR search executed in %f seconds", (time.gps() - atl03_cmr_start_time) / 1000.0))
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
local atl09_polygon_cmr_start_time = time.gps()
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
userlog:alert(core.INFO, core.RTE_INFO, string.format("HLS polygon CMR search executed in %f seconds", (time.gps() - atl09_polygon_cmr_start_time) / 1000.0))

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
userlog:alert(core.INFO, core.RTE_INFO, string.format("HLS STAC search executed in %f seconds", (time.gps() - stac_start_time) / 1000.0))

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
end
parms["asset"] = original_asset
parms["t0"] = original_t0
parms["t1"] = original_t1
userlog:alert(core.INFO, core.RTE_INFO, string.format("ATL09 CMR search executed in %f seconds", (time.gps() - atl09_cmr_start_time) / 1000.0))

-- initialize container runtime environment
local crenv = runner.setup()
if not crenv.host_sandbox_directory then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("failed to initialize container runtime for %s", resource))
    return
end

-- read ICESat-2 inputs
local bathy_parms   = icesat2.bathyparms(parms)
local reader        = icesat2.atl03bathy(proc.asset, resource, args.result_q, bathy_parms, geo_parms, crenv.host_sandbox_directory, false)
local status        = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, false)
if not status then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("failed to generate ATL03 bathy inputs for %s", resource))
    runner.cleanup(crenv)
end

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
            local settings_filename = string.format("%s/%s.json", crenv.container_sandbox_mount, name)                           -- e.g. /share/openoceans.json
            local parameters_filename = string.format("%s/%s_%d.json", crenv.container_sandbox_mount, icesat2.BATHY_PREFIX, i)   -- e.g. /share/bathy_spot_3.json
            local input_filename = string.format("%s/%s_%d.csv", crenv.container_sandbox_mount, icesat2.BATHY_PREFIX, i)         -- e.g. /share/bathy_spot_3.csv
            local output_filename = string.format("%s/%s_%d.csv", crenv.container_sandbox_mount, name, i)                        -- e.g. /share/openoceans_3.csv
            local container_command = command_override or string.format("/env/bin/python /%s/runner.py", name)
            local container_parms = {
                image =  "oceaneyes",
                command = string.format("%s %s %s %s %s", container_command, settings_filename, parameters_filename, input_filename, output_filename),
                timeout = container_timeout,
                parms = { [name..".json"] = parms[name] or {void=true} }
            }
            local container = runner.execute(crenv, container_parms, rspq)
            table.insert(container_list, container)
            local spot_key = string.format("spot_%d", i)
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
    userlog:alert(core.INFO, core.RTE_INFO, string.format("%s executed in %f seconds", name, (stop_time - start_time) / 1000.0))
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
    userlog:alert(core.INFO, core.RTE_INFO, string.format("%s executed in %f seconds", name, (stop_time - start_time) / 1000.0))
end

-- capture setup time
userlog:alert(core.INFO, core.RTE_INFO, string.format("sliderule setup executed in %f seconds", (time.gps() - endpoint_start_time) / 1000.0))

-- execute medialfilter bathy
runclassifier(output_files, bathy_parms, timeout, "medianfilter", true)

-- execute cshelph bathy
runclassifier(output_files, bathy_parms, timeout, "cshelph",true)

-- execute bathypathfinder bathy
runclassifier(output_files, bathy_parms, timeout, "bathypathfinder", true)

-- execute pointnet2 bathy
runclassifier(output_files, bathy_parms, timeout, "pointnet2", false)

-- execute openoceans
runclassifier(output_files, bathy_parms, timeout, "openoceans", false)

-- execute coastnet bathy
runclassifier(output_files, bathy_parms, timeout, "coastnet", false, "bash /coastnet/bathy.sh")

-- perform refraction correction
runprocessor(bathy_parms, timeout, "atl24refraction", true)

-- perform uncertainty calculations
runprocessor(bathy_parms, timeout, "atl24uncertainty", true)

-- build final output
local output_parms = parms[arrow.PARMS] or {
    path = string.gsub(resource, "ATL03", "ATL24"),
    format = "hdf5",
    as_geo = false
}
local writer_parms = {
    image =  "oceaneyes",
    command = string.format("/env/bin/python /atl24writer/runner.py %s/writer_settings.json %s/writer_ancillary.json %s/writer_orbit.json", crenv.container_sandbox_mount, crenv.container_sandbox_mount, crenv.container_sandbox_mount),
    timeout = timeout,
    parms = { 
        ["writer_settings.json"] = {
            input_files = output_files,
            output_parms = output_parms,
            atl24_filename = crenv.container_sandbox_mount.."/atl24.bin"
        }
    }
}
local container = runner.execute(crenv, writer_parms, rspq)
runner.wait(container, timeout)

-- send final output to user
arrow.send2user(crenv.host_sandbox_directory.."/atl24.bin", arrow.parms(output_parms), rspq)

-- cleanup container runtime environment
--runner.cleanup(crenv)
