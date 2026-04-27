-------------------------------------------------------
-- initialization
-------------------------------------------------------
local parms = nil

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    return core.ams("POST", _rqst.arg, arg[1])
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Asset Metadata Service",
    description = "Pass-through to Asset Metadata Service API",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"json"}
}
