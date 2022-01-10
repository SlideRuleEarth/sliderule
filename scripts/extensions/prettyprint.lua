
function _display (var, indent)

    local toprint = ""

    for k, v in pairs(var) do
        toprint = toprint .. string.rep(" ", indent)

        if (type(k) == "number") then
            toprint = toprint .. "[" .. k .. "]:"
        elseif (type(k) == "string") then
            toprint = toprint  .. k ..  ": "
        end

        if (type(v) == "number") then
            toprint = toprint .. v .. ",\r\n"
        elseif (type(v) == "string") then
            toprint = toprint .. "\"" .. v .. "\",\r\n"
        elseif (type(v) == "table") then
            toprint = toprint .. "{\r\n" .. _display(v, indent + 2) .. ",\r\n"
        else
            toprint = toprint .. "\"" .. tostring(v) .. "\",\r\n"
        end
    end

    toprint = toprint .. string.rep(" ", indent-2) .. "}"

    return toprint
end

function display (var)
    if (type(var) ~= "table") then
        print(var)
    else
        print("{")
        print(_display(var, 2))
    end
end

local package = {
    display = display,
}

return package
