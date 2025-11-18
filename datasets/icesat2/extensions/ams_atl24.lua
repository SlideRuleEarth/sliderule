local json = require("json")

local function query (parms)
    if parms["granule"] then
        return core.ams("GET", string.format("atl24/granule/%s", parms["granule"]))
    else
        local first_parm = true
        local request_str = ""
        for _,v in ipairs({"t0", "t1", "season", "photons0", "photons1", "meandepth0", "meandepth1", "mindepth0", "mindepth1", "maxdepth0", "maxdepth1", "poly"}) do
            if parms[v] then
                request_str = request_str .. string.format("%s%s=%s", first_parm and "" or "&", v, parms[v])
                first_parm = false
            end
        end
        if #request_str > 0 then
            return core.ams("GET", string.format("atl24?%s", request_str))
        else
            return nil
        end
    end
end

local package = {
    query = query,
}

return package
