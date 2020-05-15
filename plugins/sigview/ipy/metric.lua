--------------------------------------------------------------------------------------
-- File Data  -
--------------------------------------------------------------------------------------
local recdataq = "recdataq" -- base records
local scidataq = "scidataq" -- ccsds packets
local metricq = "metricq" -- metric records
local threads = 4

--------------------------------------------------------------------------------------
-- Global Data  -
--------------------------------------------------------------------------------------
baseDispatcher = nil
ccsdsDispatcher = nil
reportDispatcher = nil
metricDictionary = {}

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
        if not baseDispatcher then
            baseDispatcher = core.dispatcher(recdataq, threads, player_key)
            baseDispatcher:name("baseDispatcher")
        end
        for recname,fieldlist in pairs(metrictable["base"]) do
            for _,fieldname in ipairs(fieldlist) do
                local metricname = string.format('%s.%sMetric', recname, fieldname)
                metricDictionary[metricname] = core.metric(fieldname, metricq)
                metricDictionary[metricname]:pbtext(true)
                metricDictionary[metricname]:pbname(true)
                numcolumns = numcolumns + 1
                columnlist[numcolumns] = string.format('%s.%s', recname, fieldname)
                baseDispatcher::attach(metricDictionary[metricname], recname)
            end
        end
    end

    -- Create CCSDS Record Metrics --
    if metrictable["ccsds"] then
        if not ccsdsDispatcher then
            ccsdsDispatcher = ccsds.dispatcher(scidataq, threads, player_key)
            ccsdsDispatcher:name("ccsdsDispatcher")
        end
        for recname,fieldlist in pairs(metrictable["ccsds"]) do
            for _,fieldname in ipairs(fieldlist) do
                metricname = string.format('%s.%sMetric', recname, fieldname)
                metricDictionary[metricname] = core.metric(fieldname, metricq)
                metricDictionary[metricname]:pbtext(true)
                metricDictionary[metricname]:pbname(true)
                numcolumns = numcolumns + 1
                columnlist[numcolumns] = string.format('%s.%s', recname, fieldname)
                cds_times_present = true
                ccsdsDispatcher::attach(metricDictionary[metricname], recname)
            end
        end
    end

    -- Create Report --
    local reportname = nil 
    if reportfilename then
        -- Sort Column List --
        table.sort(columnlist)

        -- Create Report Dispatch --
        local reportbufsize = (threads - 1) * numcolumns * 256
        if reportbufsize > 65536 then reportbufsize = 65536 end
        if not reportDispatcher then
            reportDispatcher = core.dispatcher(recdataq, threads, core.FIELD_KEY, "INDEX")
            reportDispatcher:name("reportDispatcher")
        end
        reportname = string.format('report_%s', reportfilename)
        metricDictionary[reportname] = core.report(core.CVS, reportfilename, reportbufsize, columnlist)
        if cds_times_present then metricDictionary[reportname]:idxdisplay("GMT") end
        reportDispatcher::attach(metricDictionary[reportname], "Metric")
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
        if not baseDispatcher then
            baseDispatcher = core.dispatcher(recdataq, threads, player_key)
            baseDispatcher:name("baseDispatcher")
        end
        for i,limit in ipairs(limittable["base"]) do
            local recname = limit["record"]
            local fieldname = limit["field"]
            local id = limit["id"] or nil
            local min = limit["min"] or nil
            local max = limit["max"] or nil
            local limitname = string.format('%s.%sLimit_%d', recname, fieldname, i)
            metricDictionary[limitname] = core.limit(fieldname, id, min, max)
            metricDictionary[limitname]:setloglvl(core.CRITICAL)
            baseDispatcher::attach(metricDictionary[limitname], recname)
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
            local id = limit["id"] or nil
            local min = limit["min"] or nil
            local max = limit["max"] or nil
            local limitname = string.format('%s.%sLimit_%d', recname, fieldname, i)
            metricDictionary[limitname] = core.limit(fieldname, id, min, max)
            metricDictionary[limitname]:setloglvl(core.CRITICAL)
            metricDictionary[limitname]:gmtdisplay(true)            
            ccsdsDispatcher::attach(metricDictionary[limitname], recname)
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
