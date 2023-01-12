--
-- ENDPOINT:    /source/atl08
--
-- PURPOSE:     generate customized atl08 data product
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
-- OUTPUT:      atl08rec (ATL0* algorithm results)
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl08rec' RecordObjects
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
local interval = 10 < timeout and 10 or timeout -- seconds

-- Get Asset --
local asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", atl03_asset))
    do return end
end

-- Create Record Queue --
local recq = rspq .. "-atl03"

-- ATL08 Dispatcher --
local atl08_disp = core.dispatcher(recq)

-- Request Parameters */
local rqst_parms = icesat2.parms(parms)

-- Exception Forwarding --
local except_pub = core.publish(rspq)
atl08_disp:attach(except_pub, "exceptrec") -- exception records
atl08_disp:attach(except_pub, "extrec") -- ancillary records

-- ATL08 Dispatch Algorithm --
local atl08_algo = icesat2.atl08(rspq, rqst_parms)
atl08_disp:attach(atl08_algo, "atl03rec")

-- Run ATL08 Dispatcher --
atl08_disp:run()

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request <%s> atl08 processing initiated on %s ...", rspq, resource))

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

-- Wait Until ATL08 Dispatch Completion --
while (userlog:numsubs() > 0) and not atl08_disp:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration))
        do return end
    end
    userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to process ATL03 records (after %d seconds)", rspq, duration))
end

-- Request Processing Complete
userlog:sendlog(core.INFO, string.format("request <%s> processing complete", rspq))
