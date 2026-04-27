-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local earthdata = require("earth_data_query")
local parms     = nil

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    -- build directory of assets
    local directory = {}
    local assets = earthdata.load()
    for _,asset in pairs(assets) do
        local name, identity, driver, path, index, endpoint, status = asset:info()
        if status then
            directory[name] = {name=name, identity=identity, driver=driver, path=path, index=index, endpoint=endpoint}
        end
    end
    -- return response
    return json.encode({
        directory = directory,
        drivers = core.iodrivers(),
        rasters = geo.factories()
    })
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Assets",
    logging = core.DEBUG,
    description = "Lists available assets, drivers, and rasters",
    roles = {},
    signed = false,
    outputs = {"json"}
}