local json = require("json")

local asset = core.getbyname("icesat2-atl13")
local h5obj = h5.file(asset, "ATL13_20250302152414_11692601_006_01.h5")
local column = h5obj:readp("gt1r/atl13refid")
local values = column:unique()

local results = {}
for k,v in pairs(values) do
    results[tostring(k)] = v
end

return json.encode(results)