--
-- ENDPOINT:    /source/time
--
-- INPUT:       arg[1] -
--              {
--                  "filename":     "<name of hdf5 file>"
--                  "dataset":      "<name of dataset>"
--                  "datatype":     <RecordObject::valType_t>
--                  "id":           <integer id to attach to data>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Array of integers containing the values in the dataset
--
-- NOTES:       1. The arg[1] input is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing a native integer array
--

local json = require("json")
local parm = json.decode(arg[1])

local filename = parm["filename"]
local dataset = parm["dataset"]
local datatype = parm["datatype"] or core.DYNAMIC
local id = parm["id"] or 0

f = icesat2.h5dataset(core.READER, filename, dataset, id, false, datatype)
r = core.reader(f, rspq)

sys.wait(1) -- ensures rspq contains data before returning (TODO: optimize out)

return
