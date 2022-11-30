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

-- Initialize Timeouts --
local duration = 0
local interval = 10 -- seconds

-- Get Asset --
local asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", atl03_asset))
    return
end

-- Create Record Queue --
local recq = rspq .. "-atl03"

-- Exception Forwarding --
local except_pub = core.publish(rspq)

-- Request Parameters */
local rqst_parms = icesat2.parms(parms)

-- ATL06 Dispatch Algorithm --
local atl06_algo = icesat2.atl06(rspq, rqst_parms)

-- ATL06 Dispatcher --
local atl06_disp = core.dispatcher(recq)
atl06_disp:attach(atl06_algo, "atl03rec")
atl06_disp:attach(except_pub, "exceptrec") -- exception records
atl06_disp:attach(except_pub, "extrec") -- ancillary records
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
        return
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to read %s (after %d seconds)", rspq, resource, duration))
end

-- Resource Processing Complete
local atl03_stats = atl03_reader:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing of %s complete (%d/%d/%d)", rspq, resource, atl03_stats.read, atl03_stats.filtered, atl03_stats.dropped))

-- Wait Until Dispatch Completion --
while (userlog:numsubs() > 0) and not atl06_disp:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration))
        return
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to process ATL03 records (after %d seconds)", rspq, duration))
end

-- Request Processing Complete
local atl06_stats = atl06_algo:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing complete (%d/%d/%d/%d)", rspq, atl06_stats.h5atl03, atl06_stats.filtered, atl06_stats.posted, atl06_stats.dropped))

return
