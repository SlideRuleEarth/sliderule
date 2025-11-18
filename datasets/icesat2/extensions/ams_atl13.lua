local json = require("json")

local function query (parms)
    if parms["refid"] then
        return core.ams("GET", string.format("atl13?refid=%d", parms["refid"]))
    elseif parms["name"] then
        return core.ams("GET", string.format("atl13?name=%s", string.gsub(parms["name"], " ", "%%20")))
    elseif parms["coord"]["lat"] or parms["coord"]["lon"] then
        return core.ams("GET", string.format("atl13?lon=%f&lat=%f", parms["coord"]["lon"], parms["coord"]["lat"]))
    else
        return nil
    end
end

local package = {
    query = query,
}

return package
