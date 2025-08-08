--
-- ENDPOINT:    /source/ace
--
-- INPUT:       lua code to execute
--
-- OUTPUT:      result of executed code
--
local user_code = arg[1]

-- check if private cluster
if sys.getcfg("is_public") then
    return
end

-- load user code
local loaded_code = load(user_code)
if not loaded_code then
    return
end

-- execute and return results
local result = loaded_code()
return result
