local function populate_name_filter(parms)
    local granule_parms = parms["granule"] or {}
    local rgt           = parms["rgt"] or granule_parms["rgt"]
    local cycle         = parms["cycle"] or granule_parms["cycle"]
    local region        = parms["region"] or granule_parms["region"]
    if (not parms["name_filter"]) and (rgt or cycle or region) then
        local rgt_filter = rgt and string.format("%04d", rgt) or '____'
        local cycle_filter = cycle and string.format("%02d", cycle) or '__'
        local region_filter = region and string.format("%02d", region) or '__'
        parms["name_filter"] = '\\_' .. rgt_filter .. cycle_filter .. region_filter .. '\\_'
    end
end

local package = {
    populate_name_filter = populate_name_filter,
}

return package
