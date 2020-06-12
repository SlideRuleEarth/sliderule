--
-- ENDPOINT:    /source/time
--
-- INPUT:       rqst -
--              {
--                  "filename":     "<name of hdf5 file>"
--                  "track":        <track number: 1, 2, 3>
--                  "stages":       [<algorith stage 1>, ...]
--                  "parms":
--                  {
--                      "srt":      <surface type - default = LAND ICE(3)>
--                      "cnf":      <signal confidence level - default = SURFACE HIGH(4)>
--                  }
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Algorithm results
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing the output of tha algorithm
--

local json = require("json")
local rqst = json.decode(arg[1])

-- Internal Parameters --
local str2stage = { AVG=icesat2.STAGE_AVG, LSF=icesat2.STAGE_LSF }

-- Request Parameters --
local filename = rqst["filename"]
local track = rqst["track"]
local stages = rqst["stages"]
local parms = rqst["parms"]

-- ATL06 Dispatch --
a = icesat2.atl06(rspq)
if stages then
    for k,v in pairs(stages) do
        a:select(str2stage[v])
    end
end
d = core.dispatcher("recq")
d:attach(a, "h5atl03")

-- ATL03 Data Handle --
h = icesat2.h5atl03(track)
if parms then
    h:config(parms)
end

-- ATL03 File Reader --
f = icesat2.h5file(h, core.READER, filename)
r = core.reader(f, "recq")

-- Wait for Data to Start --
sys.wait(1) -- ensures rspq contains data before returning (TODO: optimize out)

-- Display Stats --
print("ATL03", json.encode(h:stats(true)))
print("ATL06", json.encode(a:stats(true)))

return
