-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local earthdata = require("earth_data_query")
    local parm = json.decode(arg[1])
    local status,rsps = earthdata.search(parm)
    if status == earthdata.SUCCESS then
        return json.encode(rsps), true
    else
        local err_rsps = json.encode({error=rsps, code=status})
        sys.log(core.CRITICAL, "Failed earthdata request: " .. err_rsps)
        return err_rsps, false
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Earthdata",
    description = "Query earth data assets for resources to process",
    logging = core.INFO,
    roles = {},
    signed = false,
    outputs = {"json"}
}
