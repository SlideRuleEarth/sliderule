--
-- ENDPOINT:    /source/atl06
--
-- PURPOSE:     generate customized atl06 data product
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resources":    "[<url of hdf5 file or object>, ...]"
--                  "parms":        {<table of parameters>}
--                  "timeout":      <milliseconds to wait for first response>
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
local resources = rqst["resources"]
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Get Asset --
local asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", atl03_asset))
    return
end

-- Create Record Queue --
local recq = rspq .. "-atl03"
local rec_pub = core.publish(recq)

-- Exception Forwarding --
local except_pub = core.publish(rspq)

-- ATL06 Dispatch Algorithm --
local atl06_algo = icesat2.atl06(rspq, parms)

-- ATL06 Dispatcher --
local atl06_disp = core.dispatcher(recq)
atl06_disp:attach(atl06_algo, "atl03rec")
atl06_disp:attach(except_pub, "exceptrec")
atl06_disp:run()

-- Loop Through Resources --
for i,resource in ipairs(resources) do

    -- Post Initial Status Progress --
    userlog:sendlog(core.INFO, string.format("request <%s> atl06 processing initiated [%d out of %d] on %s ...", rspq, i, #resources, resource))

    -- ATL03 Reader --
    local atl03_reader = icesat2.atl03(asset, resource, recq, parms, false)

    -- Wait Until Reader Completion --
    local duration = 0
    local interval = 10000 -- 10 seconds
    while not atl03_reader:waiton(interval) do
        duration = duration + interval
        -- Check for Timeout --
        if timeout >= 0 and duration >= timeout then
            userlog:sendlog(core.ERROR, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration / 1000))
            return
        end
        userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to read %s (after %d seconds)", rspq, resource, duration / 1000))
    end

    -- Resource Processing Complete
    local atl03_stats = atl03_reader:stats(false)
    userlog:sendlog(core.INFO, string.format("request <%s> processing of %s complete (%d/%d/%d)", rspq, resource, atl03_stats.read, atl03_stats.filtered, atl03_stats.dropped))
end

-- Terminate Record Queue
rec_pub:sendstring("") -- signals completion to atl06 dispatch

-- Wait Until Dispatch Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not atl06_disp:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration / 1000))
        return
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to process ATL03 records (after %d seconds)", rspq, duration / 1000))
end

-- Request Processing Complete
local atl06_stats = atl06_algo:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing complete (%d/%d/%d/%d)", rspq, atl06_stats.h5atl03, atl06_stats.filtered, atl06_stats.posted, atl06_stats.dropped))

return
