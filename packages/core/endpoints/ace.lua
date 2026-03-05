--
-- ENDPOINT:    /source/ace
--
-- INPUT:       lua code to execute
--
-- OUTPUT:      result of executed code
--
local global = require("global")

-------------------------------------------------------
-- verify permissions
-------------------------------------------------------
if not global.membership(_rqst.orgroles) then -- check if member
    return "user must be a sliderule member to execute this endpoint", false
elseif not _rqst.signed then -- check request signature
    return "user must used a signed request for this endpoint", false
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
