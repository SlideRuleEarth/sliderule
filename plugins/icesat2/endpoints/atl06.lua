--
-- ENDPOINT:    /source/atl06
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resource":     "<url of hdf5 file or object>"
--                  "track":        <track number: 1, 2, 3>
--                  "parms":        {<table of parameters>}
--                  "timeout":      <milliseconds to wait for first response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Algorithm results
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl06rec' and 'atl06rec.elevation' RecordObjects
--

local json = require("json")
local asset = require("asset")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"] or "atlas-s3"
local resource = rqst["resource"]
local track = rqst["track"] or icesat2.ALL_TRACKS
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Check Stages --
local recq = rspq .. "-atl03"
local subset_only = false
local stages = parms["stages"]
if stages then
    -- Special Case for Only Subsetted ATL03 Data --
    for index, value in ipairs(stages) do
        if value == "SUB" then
            subset_only = true
            recq = rspq -- send data directly to response
            break
        end
    end
end

-- Post Initial Status Progress --
if not subset_only then
    userlog:sendlog(core.INFO, string.format("atl06 processing initiated on %s ...\n", resource))
else
    userlog:sendlog(core.INFO, string.format("atl06 subsetting only for %s ...\n", resource))
end

-- ATL06 Dispatch Algorithm --
local wait_obj = nil
if not subset_only then
    atl06_algo = icesat2.atl06(rspq, parms)
    atl06_algo:name("atl06_algo")

    -- ATL06 Dispatcher --
    atl06_disp = core.dispatcher(recq)
    atl06_disp:name("atl06_disp")
    atl06_disp:attach(atl06_algo, "atl03rec")
    atl06_disp:run()

    -- Set Wait Object --
    wait_obj = atl06_disp
end

-- ATL03 Reader --
local resource_url = asset.buildurl(atl03_asset, resource)
atl03_reader = icesat2.atl03(resource_url, recq, parms, track)
atl03_reader:name("atl03_reader")
wait_obj = wait_obj or atl03_reader

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not wait_obj:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        userlog:sendlog(core.INFO, string.format("request for %s timed-out after %d seconds\n", resource, duration / 1000))
        return
    end
    if not subset_only then
        -- Get Stats --
        local atl03_stats = atl03_reader:stats(false)
        local atl06_stats = atl06_algo:stats(false)
        -- Dispay Progress --
        if atl06_stats.h5atl03 == 0 then
            userlog:sendlog(core.INFO, string.format("... continuing to read %s (after %d seconds)\n", resource, duration / 1000))
        else
            userlog:sendlog(core.INFO, string.format("processed %d out of %d segments in %s (after %d seconds)\n", atl06_stats.h5atl03, atl03_stats.read, resource, duration / 1000))
        end
    else
        local atl03_stats = atl03_reader:stats(false)
        userlog:sendlog(core.INFO, string.format("read %d segments in %s (after %d seconds)\n", atl03_stats.read, resource, duration / 1000))
    end

end

-- Processing Complete
userlog:sendlog(core.INFO, string.format("processing of %s complete\n", resource))
return
