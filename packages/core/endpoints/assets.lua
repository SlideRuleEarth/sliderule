--
-- ENDPOINT:    /source/assets
--
-- INPUT:       none
--
-- OUTPUT:      json string of all the assets
--
local json = require("json")
local asset = require("asset")

local assets = asset.loaddir()
local directory =  {}
for key,obj in pairs(assets) do
    local name, identity, driver, path, index, region, endpoint = obj:info()
    directory[key] = {name=name, identity=identity, driver=driver, path=path, index=index, region=region, endpoint=endpoint}
end

local response = {
    directory = directory,
    drivers = core.iodrivers(),
    rasters = geo.factories()
}

return json.encode(response)

