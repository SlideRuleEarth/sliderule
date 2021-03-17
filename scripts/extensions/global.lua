
local function eval (attribute, package)
    local p = package or "core"
    if type(attribute) == "string" then
        local f = assert(load("return " .. string.format('%s.%s', p, attribute)))
        return f()
    else 
        return attribute
    end
end


local package = {
    eval = eval
}

return package
