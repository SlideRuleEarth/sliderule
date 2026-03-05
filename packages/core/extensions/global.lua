local json = require("json")

local function eval (attribute, package)
    local p = package or "core"
    if type(attribute) == "string" then
        local f = assert(load("return " .. string.format('%s.%s', p, attribute)))
        return f()
    else
        return attribute
    end
end

local function check (expression)
    local result = nil
    if type(expression) == "string" then
        local f = assert(load("return " .. expression))
        result = f() -- execute expression
    else
        result = expression
    end
    return result
end

local function split(str, delimiter)
    local result = {}
    for match in (str .. delimiter):gmatch("(.-)" .. delimiter) do
        table.insert(result, match)
    end
    return result
end


local function membership(roles) -- determine membership
    local is_member = false
    local status, org_roles = pcall(json.decode, roles)
    if status then
        local n = #org_roles
        for i = 1, n do
            if org_roles[i] == "member" then
                is_member = true
                break
            end
        end
    end
    return is_member
end

local package = {
    eval = eval,
    check = check,
    split = split,
    membership = membership
}

return package
