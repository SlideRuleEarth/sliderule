
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
    if type(expression) == "string" then
        local f = assert(load("return " .. expression))
        result = f() -- execute expression
    else
        result = expression
    end
    return result
end

local package = {
    eval = eval,
    check = check
}

return package
