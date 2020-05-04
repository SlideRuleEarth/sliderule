--------------------------------------------------------------------------------------
-- File Data  -
--------------------------------------------------------------------------------------
local recdataq = "recdataq" -- base records
local scidataq = "scidataq" -- ccsds packets
local metricq = "metricq" -- metric records
local threads = 4

--------------------------------------------------------------------------------------
-- createMetrics  -
--
--  metrictable:  { base: { <recname>: [<field>, ... ], ... } 
--                  ccsds: { <recname>: [<field>, ... ], ... } 
--                  key: <player key> }
--
--  reportfilename: <file name of CSV report of metric values, nil if not desired>
--------------------------------------------------------------------------------------
local function createMetrics(metrictable, reportfilename)

    local columnlist = {}
    local numcolumns = 0
    local cds_times_present = false

    -- Get Key --
    local player_key = metrictable["key"] or "RECEIPT_KEY"

    -- Create Base Record Metrics --
    if metrictable["base"] then        
        local basedis = cmd.exec('TYPE baseDispatcher')
        if basedis < 0 then
            cmd.exec(string.format('NEW RECORD_DISPATCHER baseDispatcher %s %s %d', recdataq, player_key, threads))
        end
        for recname,fieldlist in pairs(metrictable["base"]) do
            for _,fieldname in ipairs(fieldlist) do
                local metricname = string.format('%s.%sMetric', recname, fieldname)
                cmd.exec(string.format('NEW METRIC_DISPATCH %s %s %s', metricname, fieldname, metricq))
                cmd.exec(string.format('%s::PLAYBACK_TEXT ENABLE', metricname))
                cmd.exec(string.format('%s::PLAYBACK_NAME ENABLE', metricname))
                numcolumns = numcolumns + 1
                columnlist[numcolumns] = string.format('%s.%s', recname, fieldname)
                cmd.exec(string.format('baseDispatcher::ATTACH_DISPATCH %s %s', metricname, recname))
            end
        end
    end

    -- Create CCSDS Record Metrics --
    if metrictable["ccsds"] then
        local ccsdsdis = cmd.exec('TYPE ccsdsDispatcher')
        if ccsdsdis < 0 then
            cmd.exec(string.format('NEW CCSDS_RECORD_DISPATCHER ccsdsDispatcher %s %s %d', scidataq, player_key, threads))
        end
        for recname,fieldlist in pairs(metrictable["ccsds"]) do
            for _,fieldname in ipairs(fieldlist) do
                metricname = string.format('%s.%sMetric', recname, fieldname)
                cmd.exec(string.format('NEW METRIC_DISPATCH %s %s %s', metricname, fieldname, metricq))
                cmd.exec(string.format('%s::PLAYBACK_TEXT ENABLE', metricname))
                cmd.exec(string.format('%s::PLAYBACK_NAME ENABLE', metricname))
                numcolumns = numcolumns + 1
                columnlist[numcolumns] = string.format('%s.%s', recname, fieldname)
                cds_times_present = true
                cmd.exec(string.format('ccsdsDispatcher::ATTACH_DISPATCH %s %s', metricname, recname))
            end
        end
    end

    -- Create Report --
    local reportname = nil 
    if reportfilename then
        -- Create Column List --
        columns = ""
        table.sort(columnlist)
        for _,column in ipairs(columnlist) do
            columns = columns .. " " .. column
        end
        if string.len(columns) > 900 then
            columns = "" -- too long for command processor, so set back to nothing and let metricWriter dynamically create header
        end    

        -- Create Report Dispatch --
        local reportbufsize = (threads - 1) * numcolumns * 256
        if reportbufsize > 65536 then reportbufsize = 65536 end
        local reportdis = cmd.exec('TYPE reportDispatcher')
        if reportdis < 0 then
            cmd.exec(string.format('NEW RECORD_DISPATCHER reportDispatcher %s FIELD_KEY INDEX', metricq))
        end
        reportname = string.format('report_%s', reportfilename)
        cmd.exec(string.format('NEW REPORT_DISPATCH %s CSV %s %d %s', reportname, reportfilename, reportbufsize, columns))
        if cds_times_present then cmd.exec(string.format('%s::SET_INDEX_DISPLAY GMT', reportname)) end
        cmd.exec(string.format('reportDispatcher::ATTACH_DISPATCH %s Metric', reportname))
    end

    return reportname

end

--------------------------------------------------------------------------------------
-- createLimits  -
--
--  limittable:  { base: [ { "record": <record>, "field": <field>, "id": <id>, "min": <min>, "max": <max> }, ... ],
--                 ccsds: [ { "record": <record>, "field": <field>, "id": <id>, "min": <min>, "max": <max> }, ... ],
--                 key: <player key> }
--------------------------------------------------------------------------------------
local function createLimits (limittable)

    -- Get Key --
    local player_key = limittable["key"] or "RECEIPT_KEY"

    -- Create Base Record Metrics --
    if limittable["base"] then        
        local basedis = cmd.exec('TYPE baseDispatcher')
        if basedis < 0 then
            cmd.exec(string.format('NEW RECORD_DISPATCHER baseDispatcher %s %s %d', recdataq, player_key, threads))
        end
        for i,limit in ipairs(limittable["base"]) do
            local recname = limit["record"]
            local fieldname = limit["field"]
            local id = limit["id"] or "ALL"
            local min = limit["min"] or "ALL"
            local max = limit["max"] or "ALL"
            local limitname = string.format('%s.%sLimit_%d', recname, fieldname, i)
            cmd.exec(string.format('NEW LIMIT_DISPATCH %s %s %s %s %s NULL NULL', limitname, id, field, min, max))
            cmd.exec(string.format('%s:SET_LOG_LEVEL CRITICAL', limitname))
            cmd.exec(string.format('baseDispatcher::ATTACH_DISPATCH %s %s', limitname, recname))
        end
    end

    -- Create CCSDS Record Metrics --
    if limittable["ccsds"] then
        local ccsdsdis = cmd.exec('TYPE ccsdsDispatcher')
        if ccsdsdis < 0 then
            cmd.exec(string.format('NEW CCSDS_RECORD_DISPATCHER ccsdsDispatcher %s %s %d', scidataq, player_key, threads))
        end
        for i,limit in ipairs(limittable["ccsds"]) do
            local recname = limit["record"]
            local fieldname = limit["field"]
            local id = limit["id"] or "ALL"
            local min = limit["min"] or "ALL"
            local max = limit["max"] or "ALL"
            local limitname = string.format('%s.%sLimit_%d', recname, fieldname, i)
            cmd.exec(string.format('NEW LIMIT_DISPATCH %s %s %s %s %s NULL NULL', limitname, id, fieldname, min, max))
            cmd.exec(string.format('%s:SET_GMT_DISPLAY ENABLE', limitname))
            cmd.exec(string.format('%s:SET_LOG_LEVEL CRITICAL', limitname))
            cmd.exec(string.format('ccsdsDispatcher::ATTACH_DISPATCH %s %s', limitname, recname))
        end
    end

end

--------------------------------------------------------------------------------------
-- Return Local Package
--------------------------------------------------------------------------------------
local package = {
    recdataq = recdataq,
    scidataq = scidataq,
    metricq = metricq,
    threads = threads,
    createMetrics = createMetrics,
    createLimits = createLimits
}

return package
