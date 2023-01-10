--
-- ENDPOINT:    /source/atl06
--
-- PURPOSE:     generate customized atl06 data product
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resource":     "<url of hdf5 file or object>"
--                  "parms":        {<table of parameters>}
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      atl06rec (ATL06 algorithm results)
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl06rec' and 'atl06rec.elevation' RecordObjects
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"] or "nsidc-s3"
local resource = rqst["resource"]
local parms = rqst["parms"]
local timeout = parms["node-timeout"] or parms["timeout"] or icesat2.NODE_TIMEOUT
local samples = parms["samples"]

-- Initialize Timeouts --
local duration = 0
local interval = 10 < timeout and 10 or timeout -- seconds

-- Get Asset --
local asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", atl03_asset))
    do return end
end

-- Create Record Queue --
local recq = rspq .. "-atl03"

-- ATL06 Dispatcher --
local atl06_disp = core.dispatcher(recq)

-- Request Parameters */
local rqst_parms = icesat2.parms(parms)

-- Exception Forwarding --
local except_pub = core.publish(rspq)
atl06_disp:attach(except_pub, "exceptrec") -- exception records
atl06_disp:attach(except_pub, "extrec") -- ancillary records

-- ATL06 Dispatch Algorithm --
local atl06_algo = icesat2.atl06(rspq, rqst_parms)
atl06_disp:attach(atl06_algo, "atl03rec")

-- Raster Sampler --
local sampler_disp = nil
if samples then
    local atl06_rec_type = parms["compact"] and "atl06rec-compact" or "atl06rec"
    local elevation_rec_type = parms["compact"] and "atl06rec-compact.elevation" or "atl06rec.elevation"
    sampler_disp = core.dispatcher(rspq, 1) -- 1 thread required until VrtRaster is thread safe
    for key,raster in pairs(samples) do
        local vrt = geo.vrt(raster["asset"], raster["algorithm"], raster["radius"], raster["zonal_stats"])
        if vrt then
            local sampler = icesat2.sampler(vrt, key, rspq, elevation_rec_type, "extent_id", "lon", "lat")
            if sampler then
                sampler_disp:attach(sampler, atl06_rec_type)
            else
                userlog:sendlog(core.CRITICAL, string.format("request <%s> failed to create sampler %s for %s", rspq, key, resource))
            end
        else
            userlog:sendlog(core.CRITICAL, string.format("request <%s> failed to create raster %s for %s", rspq, key, resource))
        end
    end
    sampler_disp:run()
end

-- Run ATL06 Dispatcher --
atl06_disp:run()

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request <%s> atl06 processing initiated on %s ...", rspq, resource))

-- ATL03 Reader --
local atl03_reader = icesat2.atl03(asset, resource, recq, rqst_parms, true)

-- Wait Until Reader Completion --
while (userlog:numsubs() > 0) and not atl03_reader:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration))
        do return end
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to read %s (after %d seconds)", rspq, resource, duration))
end

-- Resource Processing Complete
local atl03_stats = atl03_reader:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing of %s complete (%d/%d/%d)", rspq, resource, atl03_stats.read, atl03_stats.filtered, atl03_stats.dropped))

-- Wait Until ATL06 Dispatch Completion --
while (userlog:numsubs() > 0) and not atl06_disp:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration))
        do return end
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to process ATL03 records (after %d seconds)", rspq, duration))
end

-- Wait Until Sampler Dispatch Completion --
if sampler_disp then
    sampler_disp:aot() -- aborts on next timeout
    while (userlog:numsubs() > 0) and not sampler_disp:waiton(interval * 1000) do
        duration = duration + interval
        -- Check for Timeout --
        if timeout >= 0 and duration >= timeout then
            userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration))
            do return end
        end
        userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to sample ATL06 records (after %d seconds)", rspq, duration))
    end
end

-- Request Processing Complete
local atl06_stats = atl06_algo:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing complete (%d/%d/%d/%d)", rspq, atl06_stats.h5atl03, atl06_stats.filtered, atl06_stats.posted, atl06_stats.dropped))
