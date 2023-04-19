--
-- ENDPOINT:    /source/gedi02a
--
-- PURPOSE:     subset GEDI L2A products
--
-- INPUT:       rqst
--              {
--                  "resource":     "<url of hdf5 file or object>"
--                  "parms":        {<table of parameters>}
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      gedi02a (GEDI footprints)
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized gedi02arec RecordObjects
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local resource = rqst["resource"]
local parms = rqst["parms"]
local gedi_asset = parms["asset"]
local timeout = parms["node-timeout"] or parms["timeout"] or gedi.NODE_TIMEOUT

-- Initialize Timeouts --
local duration = 0
local interval = 10 < timeout and 10 or timeout -- seconds

-- Get Asset --
local asset = core.getbyname(gedi_asset)
if not asset then
    userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", gedi_asset))
    do return end
end

-- Request Parameters */
local rqst_parms = gedi.parms(parms)

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request <%s> gedi02a processing initiated on %s ...", rspq, resource))

-- GEDI L2A Reader --
local gedi02a_reader = gedi.gedi02a(asset, resource, rspq, rqst_parms, true)

-- Wait Until Reader Completion --
while (userlog:numsubs() > 0) and not gedi02a_reader:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration))
        do return end
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to read %s (after %d seconds)", rspq, resource, duration))
end

-- Resource Processing Complete
local stats = gedi02a_reader:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing of %s complete (%d/%d/%d)", rspq, resource, stats.read, stats.filtered, stats.dropped))
