--
-- ENDPOINT:    /engine/h5
--
-- INPUT:       arg[1] -
--              {
--                  "resource":     "<url of hdf5 file or object>"
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
--              3. The output is a raw binary blob containing serialized 'h5dataset' RecordObjects
--

local json = require("json")
local asset = require("asset")
local parm = json.decode(arg[1])

local asset_name = parm["asset"]
local resource = parm["resource"]
local dataset = parm["dataset"]
local datatype = parm["datatype"] or core.DYNAMIC
local id = parm["id"] or 0

f = h5.dataset(core.READER, asset.buildurl(asset_name, resource), dataset, id, false, datatype)
r = core.reader(f, rspq)

r:waiton() -- waits until reader completes

return
