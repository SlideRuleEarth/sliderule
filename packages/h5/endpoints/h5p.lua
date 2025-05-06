--
-- ENDPOINT:    /source/h5
--
-- INPUT:       arg[1] -
--              {
--                  "asset":        "<name of asset>",
--                  "resource":     "<url of hdf5 file or object>",
--                  "datasets":
--                  [
--                      {
--                          "dataset":  "<name of dataset>",
--                          "valtype":  <RecordObject::valType_t>,
--                          "col":      [<column number>],
--                          "startrow": [<row number>],
--                          "numrows":  [<total number of rows to read>]
--                      },
--                      ...
--                  ]
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      h5file records containing values in datasets
--
-- NOTES:       1. The arg[1] input is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'h5dataset' RecordObjects
--

local json = require("json")
local parm = json.decode(arg[1])

local asset_name = parm["asset"]
local resource = parm["resource"]
local datasets = parm["datasets"]

local asset = core.getbyname(asset_name)
if not asset then
    local userlog = msg.publish(rspq)
    userlog:alert(core.INFO, core.RTE_FAILURE, string.format("invalid asset specified: %s", asset_name))
    return
end

local f = h5.file(asset, resource)
if not f then
    local userlog = msg.publish(rspq)
    userlog:alert(core.INFO, core.RTE_FAILURE, string.format("failed to open resource: %s", resource))
    return
end

f:read(datasets, rspq)

return
