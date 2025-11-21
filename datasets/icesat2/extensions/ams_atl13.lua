local utils = require("ams_utils")

local function query (_parms)
    local parms = _parms["ams"]
    local status = false
    local response = nil
    utils.populate_name_filter(parms)
    if parms["refid"] then
        status, response = core.ams("GET", string.format("atl13?refid=%d", parms["refid"]))
    elseif parms["name"] then
        status, response = core.ams("GET", string.format("atl13?name=%s", string.gsub(parms["name"], " ", "%%20")))
    elseif parms["coord"]["lat"] or parms["coord"]["lon"] then
        status, response = core.ams("GET", string.format("atl13?lon=%f&lat=%f", parms["coord"]["lon"], parms["coord"]["lat"]))
    end
    if status and response and response["refid"] then -- special case to populate refid from response
        _parms["atl13"]["refid"] = response["refid"]
    end
    return status, response
end

local package = {
    query = query,
}

return package
