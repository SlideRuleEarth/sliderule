-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local parms     = json.decode(arg[1])
local asset     = core.getbyname(parms["asset"], true)
local resource  = parms["resource"]
local dataset   = parms["dataset"]
local datatype  = parms["datatype"] or core.DYNAMIC
local col       = parms["col"] or 0
local startrow  = parms["startrow"] or 0
local numrows   = parms["numrows"] or h5coro.ALL_ROWS
local id        = parms["id"] or 0

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
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
    parms = parms,
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
