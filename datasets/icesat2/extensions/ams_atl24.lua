local json = require("json")
local utils = require("ams_utils")

local function query (_parms)
    local parms = _parms["ams"]
    if parms["granule"] then
        return core.ams("GET", string.format("atl24/granule/%s", parms["granule"]))
    else
        utils.populate_name_filter(parms)
        local first_parm = true
        local request_str = ""
        for _,v in ipairs({ "t0", "t1", "poly",
                            "season", "photons0", "photons1", "meandepth0", "meandepth1", "mindepth0", "mindepth1", "maxdepth0", "maxdepth1",
                            "name_filter" }) do
            if parms[v] then
                request_str = request_str .. string.format("%s%s=%s", first_parm and "" or "&", v, parms[v])
                first_parm = false
            end
        end
        if #request_str > 0 then
            return core.ams("GET", string.format("atl24?%s", request_str))
        else
            return false, nil
        end
    end
end

local package = {
    query = query,
}

return package
