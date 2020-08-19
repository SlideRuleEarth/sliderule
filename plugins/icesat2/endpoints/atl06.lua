--
-- ENDPOINT:    /engine/atl06
--
-- INPUT:       rqst
--              {
--                  "asset":        "<name of asset to use, defaults to atl03-local>"
--                  "resource":     "<url of hdf5 file or object>"
--                  "track":        <track number: 1, 2, 3>
--                  "stages":       [<algorith stage 1>, ...]
--                  "parms":        {<table of parameters>}
--                  "timeout":      <seconds to wait for first response>
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
local recq = "recq"

-- Create User Log --
local userlog = core.logger(rspq, core.USER, true)

-- Request Parameters --
local rqst = json.decode(arg[1])
local asset_name = rqst["asset"] or "atl03-cloud"
local resource = rqst["resource"]
local track = rqst["track"] or icesat2.ALL_TRACKS
local stages = rqst["stages"]
local parms = rqst["parms"]
local timeout = rqst["timeout"] or 100

-- Post Initial Status Progress --
sys.log(core.USER, string.format("atl06 processing initiated on %s data...", asset_name))

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

-- Response Monitor --
local monitor = msg.subscribe(rspq)

-- ATL03 Reader --
local resource_url = asset.buildurl(asset_name, resource)
atl03_reader = icesat2.atl03(resource_url, recq, parms, track)
atl03_reader:name("atl03_reader")

-- Wait for Data to Start --
local rsprec = monitor:recvrecord(timeout * 1000)
if not rsprec then
    sys.log(core.USER, string.format("Failed to generate response record in %s seconds", timeout))
end

-- Display Stats --
print("ATL03", json.encode(atl03_reader:stats(true)))
print("ATL06", json.encode(atl06_algo:stats(true)))

return
