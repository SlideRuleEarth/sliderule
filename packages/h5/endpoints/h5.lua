--
-- ENDPOINT:    /source/h5
--
-- INPUT:       arg[1] -
--              {
--                  "resource":     "<url of hdf5 file or object>"
--                  "dataset":      "<name of dataset>"
--                  "datatype":     <RecordObject::valType_t>
--                  "id":           <integer id to attach to data>
--              }
--
-- OUTPUT:      Array of integers containing the values in the dataset
--
-- NOTES:       1. The arg[1] input is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'h5dataset' RecordObjects
--

local json = require("json")
local parm = json.decode(arg[1])

local asset_name = parm["asset"]
local resource = parm["resource"]
local dataset = parm["dataset"]
local datatype = parm["datatype"] or core.DYNAMIC
local col = parm["col"] or 0
local startrow = parm["startrow"] or 0
local numrows = parm["numrows"] or h5.ALL_ROWS
local id = parm["id"] or 0

local asset = core.getbyname(asset_name)
if not asset then
    local userlog = msg.publish(_rqst.rspq)
    userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("invalid asset specified: %s", asset_name))
    return
end

local f = h5.dataset(streaming.READER, asset, resource, dataset, id, false, datatype, col, startrow, numrows)
if f:connected() then
    local r = streaming.reader(f, _rqst.rspq)
    r:waiton() -- waits until reader completes
end

return
