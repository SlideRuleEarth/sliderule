--
-- ENDPOINT:    /source/h5
--
-- INPUT:       arg[1] -
--              {
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
local asset = require("asset")
local parm = json.decode(arg[1])

local asset_name = parm["asset"]
local resource = parm["resource"]
local datasets = parm["datasets"] 

f = h5.file(asset.buildurl(asset_name, resource))
f:read(datasets, rspq)

return
