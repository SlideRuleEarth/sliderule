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

local function set(t)
    if t then
        local s = {}
        for _, v in ipairs(t) do
            s[v] = true
        end
        return s
    else
        return nil
    end
end

local function merge(t1, t2)
    local result = {}
    for k, v in pairs(t1) do
        result[k] = v
    end
    for k, v in pairs(t2) do
        result[k] = v
    end
    return result
end

local package = {
    eval = eval,
    check = check,
    split = split,
    set = set,
    merge = merge
}

return package
