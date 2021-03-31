--
-- ENDPOINT:    /engine/h5
--
-- INPUT:       arg[1] -
--              {
--                  "resource":     "<url of hdf5 file or object>",
--                  "datasets": 
--                  [
--                      {
--                          "name":     "<name of dataset>",
--                          "datatype": <RecordObject::valType_t>,
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
local datasets = parm["dataset"] 

local dataset = parm["dataset"]
local datatype = parm["datatype"] or core.DYNAMIC
local col = parm["col"] or 0
local startrow = parm["startrow"] or 0
local numrows = parm["numrows"] or h5.ALL_ROWS


f = h5.file(core.READER, asset.buildurl(asset_name, resource), dataset, id, false, datatype, col, startrow, numrows)

r:waiton() -- waits until reader completes

return
