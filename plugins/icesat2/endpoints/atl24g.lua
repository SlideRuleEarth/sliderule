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
if not crenv.unique_shared_directory then return end

-- read ICESat-2 inputs
local bathy_parms   = icesat2.bathyparms(parms)
local reader        = icesat2.atl03bathy(proc.asset, resource, args.result_q, bathy_parms, geo_parms, crenv.unique_shared_directory, false)
local status        = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, false)

-- function: generate input filenames
local function genfilenames(shared_directory, i, prefix)
    return string.format("%s/%s_%d.csv %s/%s_%d.json %s/%s_%s_%d.csv", shared_directory, icesat2.BATHY_PREFIX, i, shared_directory, icesat2.BATHY_PREFIX, i, shared_directory, prefix, icesat2.BATHY_PREFIX, i)
end

while true do
    -- abort if failed to generate atl03 bathy inputs
    if not status then break end

    -- TODO - put this in a loop for all 6 beams

    -- execute openoceans
    local openoceans_parms = {
        image =  "openoceans",
        command = "/env/bin/python /usr/local/etc/oceaneyes.py " .. genfilenames(crenv.host_shared_directory, 1, "openoceans"),
        parms = {
            ["settings.json"] = {
                var1 = 1
            }
        }
    }

    local openoceans_runner = runner.execute(openoceans_parms, crenv.unique_shared_directory, rspq)
    local openoceans_status = runner.wait(openoceans_parms, openoceans_runner)

    -- exit loop
    break
end

-- cleanup container runtime environment
--runner.cleanup(unique_shared_directory)
