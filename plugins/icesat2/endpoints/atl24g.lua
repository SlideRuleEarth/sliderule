--
-- ENDPOINT:    /source/atl24g
--

local json          = require("json")
local georesource   = require("georesource")
local earthdata     = require("earth_data_query")
local runner        = require("container_runtime")

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
local hls_parms = {
    asset       = "landsat-hls",
    t0          = t0,
    t1          = t1,
    use_poi_time = true,
    bands       = {"NDWI"},
    poly        = hls_poly
}

-- build hls raster object
local geo_parms = nil
local rc, rsps = earthdata.stac(hls_parms)
if rc == earthdata.SUCCESS then
    hls_parms["catalog"] = json.encode(rsps)
    geo_parms = geo.parms(hls_parms)
end

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
    return string.format("%s/%s.json %s/%s_%d.json %s/%s_%d.csv %s/%s_%s_%d.csv", dir, prefix, dir, icesat2.BATHY_PREFIX, i, dir, icesat2.BATHY_PREFIX, i, dir, prefix, icesat2.BATHY_PREFIX, i)
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

    -- execute openoceans
    runopenoceans(bathy_parms, timeout)

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
local openoceans_csv_files = {}
for i = 1,icesat2.NUM_SPOTS do
    if bathy_parms:spoton(i) then
        table.insert(spot_csv_files, string.format("%s/%s_%d.csv", crenv.container_shared_directory, icesat2.BATHY_PREFIX, i))
        table.insert(spot_json_files, string.format("%s/%s_%d.json", crenv.container_shared_directory, icesat2.BATHY_PREFIX, i))
        table.insert(openoceans_csv_files, string.format("%s/openoceans_%s_%d.csv", crenv.container_shared_directory, icesat2.BATHY_PREFIX, i))
    end
end
local writer_parms = {
    image =  "bathywriter",
    command = "/env/bin/python /usr/local/etc/writer.py /data/settings.json",
    timeout = timeout,
    parms = { 
        ["settings.json"] = {
            spot_csv_files = spot_csv_files,
            spot_json_files = spot_json_files,
            openoceans_csv_files = openoceans_csv_files,
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
