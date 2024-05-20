--
-- ENDPOINT:    /source/atl24g
--

local json          = require("json")
local georesource   = require("georesource")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")


local prettyprint = require("prettyprint")

-- get inputs 
local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]
local timeout       = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT

-- intialize processing environment
local args = {
    shard           = rqst["shard"] or 0, -- key space
    default_asset   = "icesat2",
    result_q        = parms[geo.PARMS] and "result." .. resource .. "." .. rspq or rspq,
    result_rec      = "bathyrec",
}
local proc = georesource.initialize(resource, parms, nil, args)

-- abort if failed to initialize
if not proc then return end

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

-- get ATL09 resources
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

-- initialize container runtime environment
local crenv = runner.setup()

-- abort if container runtime environment failed to initialize
if not crenv.host_shared_directory then return end

-- read ICESat-2 inputs
local bathy_parms   = icesat2.bathyparms(parms)
local reader        = icesat2.atl03bathy(proc.asset, resource, args.result_q, bathy_parms, geo_parms, crenv.host_shared_directory, false)
local status        = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, false)

-- function: generate input filenames
local function genfilenames(dir, i, prefix)
    -- <settings-json>   <spot-input-json>   <spot-input-csv>   <spot-output-csv>
    return string.format("%s/%s.json %s/%s_%d.json %s/%s_%d.csv %s/%s_%s_%d.csv", dir, prefix, dir, icesat2.BATHY_PREFIX, i, dir, icesat2.BATHY_PREFIX, i, dir, prefix, icesat2.BATHY_PREFIX, i)
end

-- function: run coastnet
local function runcoastnet(_bathy_parms, container_timeout)
    local container_list = {}
    for i = 1,icesat2.NUM_SPOTS do
        if _bathy_parms:spoton(i) then
            local container_parms = {
                image =  "coastnet",
                command = "bash /surface.sh " .. genfilenames(crenv.container_shared_directory, i, "coastnet"),
                timeout = container_timeout,
                parms = { ["coastnet.json"] = parms["coastnet"] }
            }
            local container = runner.execute(crenv, container_parms, rspq)
            table.insert(container_list, container)
        end
    end
    for _,container in ipairs(container_list) do
        runner.wait(container, container_timeout)        
    end
end

-- function: run openoceans
local function runopenoceans(_bathy_parms, container_timeout)
    for i = 1,icesat2.NUM_SPOTS do
        if _bathy_parms:spoton(i) then
            local container_parms = {
                image =  "openoceans",
                command = "/env/bin/python /usr/local/etc/oceaneyes.py " .. genfilenames(crenv.container_shared_directory, i, "openoceans"),
                timeout = container_timeout,
                parms = { ["openoceans.json"] = parms["openoceans"] }
            }
            local container = runner.execute(crenv, container_parms, rspq)
            runner.wait(container, container_timeout)
        end
    end
end

while true do
    -- abort if failed to generate atl03 bathy inputs
    if not status then break end

    -- execute coastnet surface
    runcoastnet(bathy_parms, timeout)

    -- execute openoceans
--    runopenoceans(bathy_parms, timeout)

    -- exit loop
    break
end

-- get output parms
local atl24_granule_filename = string.gsub(resource, "ATL03", "ATL24")
local output_parms = parms[arrow.PARMS] or {
    path = "/tmp/"..atl24_granule_filename,
    format = "hdf5",
    as_geo = false
}

-- build final output
local spot_csv_files = {}
local spot_json_files = {}
local ensemble_csv_files = {}
for i = 1,icesat2.NUM_SPOTS do
    if bathy_parms:spoton(i) then
        table.insert(spot_csv_files, string.format("%s/%s_%d.csv", crenv.container_shared_directory, icesat2.BATHY_PREFIX, i))
        table.insert(spot_json_files, string.format("%s/%s_%d.json", crenv.container_shared_directory, icesat2.BATHY_PREFIX, i))
        table.insert(ensemble_csv_files, string.format("%s/ensemble_csv_files_%s_%d.csv", crenv.container_shared_directory, icesat2.BATHY_PREFIX, i))
    end
end
local writer_parms = {
    image =  "bathywriter",
    command = "/env/bin/python /usr/local/etc/writer.py /data/writer_settings.json",
    timeout = timeout,
    parms = { 
        ["writer_settings.json"] = {
            spot_csv_files = spot_csv_files,
            spot_json_files = spot_json_files,
            ensemble_csv_files = ensemble_csv_files,
            output = output_parms,
            output_filename = crenv.container_shared_directory.."/"..atl24_granule_filename
        }
    }
}
local container = runner.execute(crenv, writer_parms, rspq)
runner.wait(container, timeout)

-- send final output to user
arrow.send2user(crenv.host_shared_directory.."/"..atl24_granule_filename, arrow.parms(output_parms), rspq)

-- cleanup container runtime environment
--runner.cleanup(crenv)
