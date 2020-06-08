--
-- ENDPOINT:    /source/time
--
-- INPUT:       arg[1] -
--              {
--                  "filename":     "<name of hdf5 file>"
--                  "track":        <track number: 1, 2, 3>
--                  "id":           <integer id to attach to data>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Algorithm results
--
-- NOTES:       1. The arg[1] input is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing the output of tha algorithm
--

local json = require("json")
local parm = json.decode(arg[1])

-- Request Parameters --
local filename = parm["filename"]
local track = parm["track"]
local id = parm["id"] or 0

-- ATL06 Dispatch --
algo = icesat2.atl06(rspq)
d = core.dispatcher("recq")
d:attach(algo, "h5rec")

-- ATL03 Inputs --
h = icesat2.h5atl03(track, id, false)
f = icesat2.h5file(h, core.READER, filename)
r = core.reader(f, "recq")

sys.wait(1) -- ensures rspq contains data before returning (TODO: optimize out)

return
