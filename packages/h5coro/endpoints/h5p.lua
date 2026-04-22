-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local parm = json.decode(arg[1])

    local asset_name = parm["asset"]
    local resource = parm["resource"]
    local datasets = parm["datasets"]

    local asset = core.getbyname(asset_name)
    if not asset then
        local userlog = msg.publish(_rqst.rspq)
        userlog:alert(core.INFO, core.RTE_FAILURE, string.format("invalid asset specified: %s", asset_name))
        return
    end

    local f = h5coro.file(asset, resource)
    if not f then
        local userlog = msg.publish(_rqst.rspq)
        userlog:alert(core.INFO, core.RTE_FAILURE, string.format("failed to open resource: %s", resource))
        return
    end

    f:read(datasets, _rqst.rspq)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "H5Coro File Read",
    description = "Read values from an HDF5 object using an H5Coro file reader (p-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary"}
}

-- INPUT
--  {
--      "asset":        "<name of asset>",
--      "resource":     "<url of hdf5 file or object>",
--      "datasets":
--      [
--          {
--              "dataset":  "<name of dataset>",
--              "valtype":  <RecordObject::valType_t>,
--              "col":      [<column number>],
--              "startrow": [<row number>],
--              "numrows":  [<total number of rows to read>],
--              "slice":    [ [<start dim0>, <end dim0>], [<start dim1>, <end dim1>], ... ]
--          },
--          ...
--      ]
--  }
