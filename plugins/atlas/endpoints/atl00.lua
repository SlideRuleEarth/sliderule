--
-- ENDPOINT:    /source/atl00
--
-- INPUT:       rqst
--              {
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Processed ATL00 data
--
--
local json = require("json")
local asset = require("asset")
local packet = require("packet")
local rqst = json.decode(arg[1])

--------------------------------------------------------------------------------------
-- File Data
--------------------------------------------------------------------------------------
local threads = rqst["threads"] or 4
local timeout_seconds = rqst["timeout"] or -1
local baseDispatcher = nil
local ccsdsDispatcher = nil
local reportDispatcher = nil
local metricDictionary = {}

--------------------------------------------------------------------------------------
-- createMetrics
--
--  metrictable:  { base: { <recname>: [<field>, ... ], ... } 
--                  ccsds: { <recname>: [<field>, ... ], ... } 
--                  key: <player key> }
--
--  reportfilename: <file name of CSV report of metric values, nil if not desired>
--------------------------------------------------------------------------------------
local function createMetrics(metrictable)
    local columnlist = {}
    local numcolumns = 0
    local cds_times_present = false

    -- Get Report Filename --
    reportfilename = "metric.csv"
    if metrictable["filename"] then
        reportfilename = metrictable["filename"]
    end
    
    -- Get Key --
    local key_str = metrictable["key"] or "RECEIPT_KEY"
    chunks = {}
    for substring in key_str:gmatch("%S+") do
        table.insert(chunks, substring)
    end
    local player_key = chunks[1]
    local key_parm = chunks[2]

    -- Create Base Record Metrics --
    if metrictable["base"] then  
        if not baseDispatcher then
            baseDispatcher = core.dispatcher("recdataq", threads, player_key, key_parm):name("baseDispatcher")
        end
        for recname,fieldlist in pairs(metrictable["base"]) do
            for _,fieldname in ipairs(fieldlist) do
                local metricname = string.format('%s.%sMetric', recname, fieldname)
                metricDictionary[metricname] = core.metric(fieldname, "metricq")
                metricDictionary[metricname]:pbtext(true)
                metricDictionary[metricname]:pbname(true)
                numcolumns = numcolumns + 1
                columnlist[numcolumns] = string.format('%s.%s', recname, fieldname)
                baseDispatcher:attach(metricDictionary[metricname], recname)
            end
        end
    end

    -- Create CCSDS Record Metrics --
    if metrictable["ccsds"] then
        if not ccsdsDispatcher then
            ccsdsDispatcher = ccsds.dispatcher("scidataq", threads, player_key, key_parm):name("ccsdsDispatcher")
        end
        for recname,fieldlist in pairs(metrictable["ccsds"]) do
            for _,fieldname in ipairs(fieldlist) do
                metricname = string.format('%s.%sMetric', recname, fieldname)
                metricDictionary[metricname] = core.metric(fieldname, "metricq")
                metricDictionary[metricname]:pbtext(true)
                metricDictionary[metricname]:pbname(true)
                numcolumns = numcolumns + 1
                columnlist[numcolumns] = string.format('%s.%s', recname, fieldname)
                cds_times_present = true
                ccsdsDispatcher:attach(metricDictionary[metricname], recname)
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
            reportDispatcher = core.dispatcher("metricq", threads, "FIELD_KEY", "INDEX")
        end
        reportname = string.format('report_%s', reportfilename)
        metricDictionary[reportname] = core.report("CSV", reportfilename, reportbufsize, columnlist)
        if cds_times_present then metricDictionary[reportname]:idxdisplay("GMT") end
        reportDispatcher:attach(metricDictionary[reportname], "Metric")
    end

    return metricDictionary[reportname]
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
    local key_str = limittable["key"] or "RECEIPT_KEY"
    chunks = {}
    for substring in key_str:gmatch("%S+") do
        table.insert(chunks, substring)
    end
    local player_key = chunks[1]
    local key_parm = chunks[2]

    -- Create Base Record Metrics --
    if limittable["base"] then        
        if not baseDispatcher then
            baseDispatcher = core.dispatcher("recdataq", threads, player_key, key_parm)
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
            baseDispatcher:attach(metricDictionary[limitname], recname)
        end
    end

    -- Create CCSDS Record Metrics --
    if limittable["ccsds"] then
        if not ccsdsDispatcher then
            ccsdsDispatcher = ccsds.dispatcher("scidataq", threads, player_key, key_parm)
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
            ccsdsDispatcher:attach(metricDictionary[limitname], recname)
        end
    end
end

--------------------------------------------------------------------------------------
-- Run Dispatchers
--------------------------------------------------------------------------------------
local function runDispatchers ()
    if baseDispatcher then baseDispatcher:run() end
    if ccsdsDispatcher then ccsdsDispatcher:run() end
    if reportDispatcher then reportDispatcher:run() end
end


--####################################################################################
-- Main
--####################################################################################

-- Create Response Publisher --
local userlog = msg.publish(rspq)

-- Forward Results
if not rqst["silent"] then
    resultBridge = core.bridge("recdataq", rspq)
end

-- Create Metrics --
if rqst["metric"] then
    report = createMetrics(rqst["metric"])
end

-- Create Limits --
if rqst["limit"] then
    createLimits(rqst["limit"])
end

-- Create Writers --
if rqst["writer"] then
    writer = rqst["writer"]
    local wformat = writer["format"]
    local wfile = writer["file"]
    cmd.exec(string.format("NEW ATLAS_FILE_WRITER atlasWriter %s %s %s", wformat, wfile, "recdataq"))
end

-- Execute Post Configuration Commands --
if rqst["postcfg"] then
    for index,cfgcmd in ipairs(rqst["postcfg"]) do
        cmd.exec(cfgcmd)
    end
end

-- Run Metrics --
runDispatchers()

-- Establish Data Source --
local source = rqst["source"]
if source["type"] == "ATL00" then
    filelist = source["files"]
    -- Build Input Queue List and Create Parsers --
    ccsdsParsers = {}
    ccsdsQs = {}
    fileQs = {}
    for index,filepath in ipairs(filelist) do
        fileq = "filedataq-"..tostring(index)
        ccsdsq = "ccsdsdataq-"..tostring(index)
        ccsdsParsers[filepath] = ccsds.parser(ccsds.pktmod(), ccsds.SPACE, fileq, ccsdsq):name(filepath)
        table.insert(ccsdsQs, ccsdsq)
        table.insert(fileQs, fileq)
    end
    -- Create Interleaver --
    interleaver = ccsds.interleaver(ccsdsQs, "scidataq")
    if source["start"] then
        interleaver:start(source["start"])
    end
    if source["stop"] then
        interleaver:stop(source["stop"])
    end
    -- Create Readers --
    sourceFiles = {}
    sourceFileReaders = {}
    for index,filepath in ipairs(filelist) do
        sourceFiles[filepath] = core.file(core.READER, core.BINARY, filepath)
        sourceFileReaders[filepath] = core.reader(sourceFiles[filepath], fileQs[index])
    end
    -- Wait for Readers to Finish Reading --
    for index,filepath in ipairs(filelist) do
        sourceFileReaders[filepath]:waiton(core.PEND)
        userlog:sendlog(core.INFO, "Finished reading "..filepath.." ["..tostring(index).."]")
    end
    -- Wait for Parsers to Finish Parsing --
    for index,filepath in ipairs(filelist) do
        while true do
            if ccsdsParsers[filepath]:waiton(5000) then
                break
            end
            userlog:sendlog(core.INFO, "Waiting on parser for "..filepath)
        end
        userlog:sendlog(core.INFO, "Finished parsing "..filepath.." ["..tostring(index).."]")
    end
end

-- Wait for Data to Finish --
cmd.exec(string.format('WAIT_ON_EMPTY %s 2', "scidataq"), timeout_seconds)
cmd.exec(string.format("WAIT_ON_EMPTY %s 2", "metricq"), timeout_seconds)
if report then
    report:flushrow('ALL')
end

-- Execute Post Termination Commands --
if rqst["postterm"] then
    for index,termcmd in ipairs(rqst["postterm"]) do
        cmd.exec(termcmd)
    end
end

-- Notify User of Processing Completion --
userlog:sendlog(core.INFO, string.format("ATL00 processing complete"))

return
