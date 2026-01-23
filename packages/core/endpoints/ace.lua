--
-- ENDPOINT:    /source/ace
--
-- INPUT:       lua code to execute
--
-- OUTPUT:      result of executed code
--
local json      = require("json")
local user_code = arg[1]
local org_roles = json.encode(_rqst.orgroles)

-- determine membership
local is_member = false
local n = #org_roles
for i = 1, n do
    if org_roles[i] == "member" then
        is_member = true
    end
end

-- check if member
if not is_member then
    return "user must be a sliderule member to execute this endpoint", false
end

-- load user code
local loaded_code = load(user_code)
if not loaded_code then
    return "unable to load user provided code", false
end

-- execute and return results
local result = loaded_code()
return result, true
