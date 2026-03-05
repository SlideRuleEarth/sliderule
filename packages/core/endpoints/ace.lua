--
-- ENDPOINT:    /source/ace
--
-- INPUT:       lua code to execute
--
-- OUTPUT:      result of executed code
--

-------------------------------------------------------
-- verify permissions
-------------------------------------------------------
if not sys.getcfg("trusted_environment") then
    -- check if member
    local global = require("global")
    if not global.membership(_rqst.orgroles) then
        return "user must be a sliderule member to execute this endpoint", false
    end
    -- check request signature
    if not _rqst.signed then
        return "user must used a signed request for this endpoint", false
    end
end

-------------------------------------------------------
-- load user code
-------------------------------------------------------
local user_code = arg[1]
local loaded_code = load(user_code)
if not loaded_code then
    return "unable to load user provided code", false
end

-------------------------------------------------------
-- execute and return results
-------------------------------------------------------
local result = loaded_code()
return result, true
