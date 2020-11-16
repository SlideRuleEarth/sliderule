--
-- ENDPOINT:    /source/atl06
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atl03-cloud>"
--                  "resource":     "<url of hdf5 file or object>"
--                  "track":        <track number: 1, 2, 3>
--                  "stages":       [<algorith stage 1>, ...]
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

-- Internal Parameters --
local str2stage = { LSF=icesat2.STAGE_LSF }
local recq = rspq .. "-atl03"

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"] or "atl03-cloud"
local resource = rqst["resource"]
local track = rqst["track"] or icesat2.ALL_TRACKS
local stages = rqst["stages"]
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("atl06 processing initiated on %s ...\n", resource))

-- ATL06 Dispatch Algorithm --
local atl06_algo = icesat2.atl06(rspq, parms)
atl06_algo:name("atl06_algo")
if stages then
    atl06_algo:select(icesat2.ALL_STAGES, false)
    for k,v in pairs(stages) do
        atl06_algo:select(str2stage[v], true)
    end
end

-- ATL06 Dispatcher --
atl06_disp = core.dispatcher(recq)
atl06_disp:name("atl06_disp")
atl06_disp:attach(atl06_algo, "atl03rec")
atl06_disp:run()

-- ATL03 Reader --
local resource_url = asset.buildurl(atl03_asset, resource)
atl03_reader = icesat2.atl03(resource_url, recq, parms, track)
atl03_reader:name("atl03_reader")

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not atl06_disp:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        userlog:sendlog(core.INFO, string.format("request for %s timed-out after %d seconds\n", resource, duration / 1000))
        return
    end
    -- Get Stats --
    local atl03_stats = atl03_reader:stats(false)
    local atl06_stats = atl06_algo:stats(false)
    -- Dispay Progress --
    if atl06_stats.h5atl03 == 0 then
        userlog:sendlog(core.INFO, string.format("... continuing to read %s (after %d seconds)\n", resource, duration / 1000))
    else
        userlog:sendlog(core.INFO, string.format("processed %d out of %d segments in %s (after %d seconds)\n", atl06_stats.h5atl03, atl03_stats.read, resource, duration / 1000))
    end
end

-- Processing Complete
userlog:sendlog(core.INFO, string.format("processing of %s complete\n", resource))
return
