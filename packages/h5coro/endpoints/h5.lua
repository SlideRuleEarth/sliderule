-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local parm = json.decode(arg[1])

    local asset_name = parm["asset"]
    local resource = parm["resource"]
    local dataset = parm["dataset"]
    local datatype = parm["datatype"] or core.DYNAMIC
    local col = parm["col"] or 0
    local startrow = parm["startrow"] or 0
    local numrows = parm["numrows"] or h5coro.ALL_ROWS
    local id = parm["id"] or 0

    local asset = core.getbyname(asset_name)
    if not asset then
        local userlog = msg.publish(_rqst.rspq)
        userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("invalid asset specified: %s", asset_name))
        return
    end

    local f = h5coro.dataset(streaming.READER, asset, resource, dataset, id, false, datatype, col, startrow, numrows)
    if f:connected() then
        local r = streaming.reader(f, _rqst.rspq)
        r:waiton() -- waits until reader completes
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "H5Coro Dataset Read",
    description = "Read values from an HDF5 object using an H5Coro streaming dataset reader",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary"}
}

-- INPUT
--  {
--      "resource":     "<url of hdf5 file or object>"
--      "dataset":      "<name of dataset>"
--      "datatype":     <RecordObject::valType_t>
--      "id":           <integer id to attach to data>
--  }
