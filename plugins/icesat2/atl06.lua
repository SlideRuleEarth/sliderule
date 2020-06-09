--
-- ENDPOINT:    /source/time
--
-- INPUT:       rqst -
--              {
--                  "filename":     "<name of hdf5 file>"
--                  "track":        <track number: 1, 2, 3>
--                  "id":           <integer id to attach to data>
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

-- Request Parameters --
local filename = rqst["filename"]
local track = rqst["track"]
local id = rqst["id"] or 0
local parms = rqst["parms"]

-- ATL06 Dispatch --
algo = icesat2.atl06(rspq)
d = core.dispatcher("recq")
d:attach(algo, "h5atl03")

-- ATL03 Data Handle --
h = icesat2.h5atl03(track)
if parms then
    h:config(parms)
end

-- ATL03 File Reader --
f = icesat2.h5file(h, core.READER, filename)
r = core.reader(f, "recq")

sys.wait(1) -- ensures rspq contains data before returning (TODO: optimize out)

return
