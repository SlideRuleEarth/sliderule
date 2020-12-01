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
-- File Data  -
--------------------------------------------------------------------------------------
local threads = rqst["threads"] or 4
local timeout_seconds = rqst["timeout"] or 3
local database = rqst["database"]
local scidataq = "scidataq" -- ccsds packets
local metricq = "metricq" -- metric records
local baseDispatcher = nil
local ccsdsDispatcher = nil
local reportDispatcher = nil
local metricDictionary = {}

--------------------------------------------------------------------------------------
-- createMetrics  -
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
            baseDispatcher = core.dispatcher(rspq, threads, player_key, key_parm)
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
                baseDispatcher:attach(metricDictionary[metricname], recname)
            end
        end
    end

    -- Create CCSDS Record Metrics --
    if metrictable["ccsds"] then
        if not ccsdsDispatcher then
            ccsdsDispatcher = ccsds.dispatcher(scidataq, threads, player_key, key_parm)
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
            reportDispatcher = core.dispatcher(metricq, threads, "FIELD_KEY", "INDEX")
            reportDispatcher:name("reportDispatcher")
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
            baseDispatcher = core.dispatcher(rspq, threads, player_key, key_parm)
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
            baseDispatcher:attach(metricDictionary[limitname], recname)
        end
    end

    -- Create CCSDS Record Metrics --
    if limittable["ccsds"] then
        if not ccsdsDispatcher then
            ccsdsDispatcher = ccsds.dispatcher(scidataq, threads, player_key, key_parm)
            ccsdsDispatcher:name("ccsdsDispatcher")
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
-- runDispatchers  -
--------------------------------------------------------------------------------------
local function runDispatchers ()
    if baseDispatcher then baseDispatcher:run() end
    if ccsdsDispatcher then ccsdsDispatcher:run() end
    if reportDispatcher then reportDispatcher:run() end
end

-----------------------------
-- Set Queue Depth
-----------------------------
local function setQDepth (_depth)
    local depth = _depth or 50000
    cmd.exec(string.format('STREAM_QDEPTH %d', depth))
    return true
end

-----------------------------
-- Start Packet Processor
-----------------------------
local function startPacketProcessor (_num_threads)
    local num_threads = _num_threads or 4
    cmd.exec(string.format('NEW CCSDS_PACKET_PROCESSOR pktProc %s %d', scidataq, num_threads))
    return true
end

-----------------------------
-- Start Logs
-----------------------------
local function startLogs ()
    sbcdiaglog = core.writer(core.file(core.WRITER, core.TEXT, "sbcdiag.log", core.FLUSHED), "sbcdiaglogq")
    pce1diaglog = core.writer(core.file(core.WRITER, core.TEXT, "pce1diag.log", core.FLUSHED), "pce1diaglogq")
    pce2diaglog = core.writer(core.file(core.WRITER, core.TEXT, "pce2diag.log", core.FLUSHED), "pce2diaglogq")
    pce3diaglog = core.writer(core.file(core.WRITER, core.TEXT, "pce3diag.log", core.FLUSHED), "pce3diaglogq")
    cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProcSbc sbcdiaglogq NULL")
    cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProc1 pce1diaglogq NULL 1")
    cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProc2 pce2diaglogq NULL 2")
    cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProc3 pce3diaglogq NULL 3")
    cmd.exec("pktProc::REGISTER   0x414 diagLogProcSbc")  -- SBC HS LOG
    cmd.exec("pktProc::REGISTER   0x43B diagLogProc1")    -- PCE 1 HS LOG
    cmd.exec("pktProc::REGISTER   0x44B diagLogProc2")    -- PCE 2 HS LOG
    cmd.exec("pktProc::REGISTER   0x45B diagLogProc3")    -- PCE 3 HS LOG
    return true
end

-----------------------------
-- Start Database
-----------------------------
local function startDatabase (_rec_path)
    local rec_path = _rec_path or "../../itos/rec/atlas/fsw/*.rec"
    cmd.exec("NEW ITOS_RECORD_PARSER itosdb")
    cmd.exec(string.format('itosdb::LOAD_REC_FILES %s', rec_path))
    cmd.exec("itosdb::SET_DESIGNATIONS  atlas_applicationId atlas_functionCode NULL")
    cmd.exec("itosdb::BUILD_DATABASE")
    cmd.exec("itosdb::BUILD_RECORDS")
    return true
end

-----------------------------
-- Start Echoes
-----------------------------
local function startEchoes ()
    cmdecho = core.writer(core.file(core.WRITER, core.TEXT, "cmdecho.log", core.FLUSHED), "cmdechoq")
    cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProc1 cmdechoq itosdb 1")
    cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProc2 cmdechoq itosdb 2")
    cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProc3 cmdechoq itosdb 3")
    cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProcSbc cmdechoq itosdb")
    cmd.exec("pktProc::REGISTER   0x42E cmdEchoProcSbc") -- SBC CMD ECHO
    cmd.exec("pktProc::REGISTER   0x43F cmdEchoProc1")   -- PCE 1 CMD ECHO
    cmd.exec("pktProc::REGISTER   0x44F cmdEchoProc2")   -- PCE 2 CMD ECHO
    cmd.exec("pktProc::REGISTER   0x45F cmdEchoProc3")   -- PCE 3 CMD ECHO
    return true
end

-----------------------------
-- Start Science Processing
-----------------------------
local function startScienceProcessing (_enTimeStat, _enPktStat, _enChStat, _enSigStat)
    -- set stats --
    local enTimeStat = _enTimeStat   or true
    local enPktStat  = _enPktStat    or false
    local enChStat   = _enChStat     or true
    local enSigStat  = _enSigStat    or true
    -- start major frame processors --
    cmd.exec("NEW MAJOR_FRAME_PROCESSOR mfProc1")
    cmd.exec("NEW MAJOR_FRAME_PROCESSOR mfProc2")
    cmd.exec("NEW MAJOR_FRAME_PROCESSOR mfProc3")
    cmd.exec("pktProc::REGISTER   0x430 mfProc1")
    cmd.exec("pktProc::REGISTER   0x440 mfProc2")
    cmd.exec("pktProc::REGISTER   0x450 mfProc3")
    -- start time processor --
    cmd.exec("NEW TIME_PROCESSOR timeProc")
    cmd.exec("pktProc::REGISTER   0x402 timeProc")  -- SIM_HK
    cmd.exec("pktProc::REGISTER   0x409 timeProc")  -- SXP_HK
    cmd.exec("pktProc::REGISTER   0x486 timeProc")  -- SXP_DIAG
    cmd.exec("pktProc::REGISTER   0x473 timeProc")  -- PCE 1 TIMEKEEPING
    cmd.exec("pktProc::REGISTER   0x474 timeProc")  -- PCE 2 TIMEKEEPING
    cmd.exec("pktProc::REGISTER   0x475 timeProc")  -- PCE 3 TIMEKEEPING
    -- start time tag processors --
    cmd.exec(string.format("NEW TIME_TAG_PROCESSOR ttProc1 %s txtimeq 1", rspq))
    cmd.exec(string.format("NEW TIME_TAG_PROCESSOR ttProc2 %s txtimeq 2", rspq))
    cmd.exec(string.format("NEW TIME_TAG_PROCESSOR ttProc3 %s txtimeq 3", rspq))
    cmd.exec("ttProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("ttProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("ttProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("ttProc1::ATTACH_TIME_PROC timeProc")
    cmd.exec("ttProc2::ATTACH_TIME_PROC timeProc")
    cmd.exec("ttProc3::ATTACH_TIME_PROC timeProc")
    cmd.exec("pktProc::REGISTER   0x4E6 ttProc1")
    cmd.exec("pktProc::REGISTER   0x4F0 ttProc2")
    cmd.exec("pktProc::REGISTER   0x4FA ttProc3")
    -- start strong altimetric processors --
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR salProc1 SAL %s 1", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR salProc2 SAL %s 2", rspq))
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR salProc3 SAL %s 3", rspq))  
    cmd.exec("salProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("salProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("salProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E2 salProc1")
    cmd.exec("pktProc::REGISTER   0x4EC salProc2")
    cmd.exec("pktProc::REGISTER   0x4F6 salProc3")
    -- start weak altimetric processors --
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR walProc1 WAL %s 1", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR walProc2 WAL %s 2", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR walProc3 WAL %s 3", rspq)) 
    cmd.exec("walProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("walProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("walProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E3 walProc1")
    cmd.exec("pktProc::REGISTER   0x4ED walProc2")
    cmd.exec("pktProc::REGISTER   0x4F7 walProc3")
    -- start strong atmospheric processors --
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR samProc1 SAM %s 1", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR samProc2 SAM %s 2", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR samProc3 SAM %s 3", rspq)) 
    cmd.exec("samProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("samProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("samProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E4 samProc1")
    cmd.exec("pktProc::REGISTER   0x4EE samProc2")
    cmd.exec("pktProc::REGISTER   0x4F8 samProc3")
    -- start weak atmospheric processors -- 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR wamProc1 WAM %s 1", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR wamProc2 WAM %s 2", rspq)) 
    cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR wamProc3 WAM %s 3", rspq)) 
    cmd.exec("wamProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("wamProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("wamProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E5 wamProc1")
    cmd.exec("pktProc::REGISTER   0x4EF wamProc2")
    cmd.exec("pktProc::REGISTER   0x4F9 wamProc3")
    -- attach statistics --
    if enTimeStat then 
        cmd.exec(string.format("timeProc.TimeStat::ATTACH %s", rspq)) 
    end
    if enPktStat then 
        cmd.exec(string.format("ttProc1.PktStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc2.PktStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc3.PktStat::ATTACH %s", rspq))
    end
    if enChStat then 
        cmd.exec(string.format("ttProc1.ChStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc2.ChStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc3.ChStat::ATTACH %s", rspq))
    end
    if enTxStat then
        cmd.exec(string.format("ttProc1.TxStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc2.TxStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc3.TxStat::ATTACH %s", rspq))
    end
    if enSigStat then
        cmd.exec(string.format("ttProc1.SigStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc2.SigStat::ATTACH %s", rspq))
        cmd.exec(string.format("ttProc3.SigStat::ATTACH %s", rspq))
    end
    -- return success -- 
    return true
end

-----------------------------
-- Start Laser
-----------------------------
local function startLaser ()
    cmd.exec("NEW LASER_PROCESSOR laserProc")
    cmd.exec("pktProc::REGISTER   0x425 laserProc") -- HKT_C (temperatures)
    cmd.exec("pktProc::REGISTER   0x427 laserProc") -- HKT_E (laser energies)
    return true
end

--####################################################################################
-- Main  -
--####################################################################################


-- Create User Status --
local userlog = msg.publish(rspq)

-- Initialize Processors --
setQDepth()
startPacketProcessor(threads)
startLogs()     
startDatabase(database)
startEchoes()     
startLaser()
startScienceProcessing()

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
    cmd.exec(string.format("NEW ATLAS_FILE_WRITER atlasWriter %s %s %s", wformat, wfile, rspq))
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
        ccsdsParsers[filepath] = ccsds.parser(ccsds.pktmod(), ccsds.SPACE, fileq, ccsdsq)
        ccsdsParsers[filepath]:name(filepath)
        table.insert(ccsdsQs, ccsdsq)
        table.insert(fileQs, fileq)
    end
    -- Create Interleaver --
    interleaver = ccsds.interleaver(ccsdsQs, scidataq)
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
        print("Finished reading "..filepath.." ["..tostring(index).."]")
    end
    -- Wait for Parsers to Finish Parsing --
    for index,filepath in ipairs(filelist) do
        while true do
            if ccsdsParsers[filepath]:waiton(5000) then
                break
            end
            print("Waiting on parser for "..filepath)
        end
        print("Finished parsing "..filepath.." ["..tostring(index).."]")
    end
end

-- Wait for Data to Finish --
cmd.exec(string.format('WAIT_ON_EMPTY %s 2', scidataq), -1) -- wait forever
cmd.exec(string.format("WAIT_ON_EMPTY %s 2", metricq), -1) -- wait forever
if report then
    report:flushrow('ALL')
end

-- Execute Post Termination Commands --
if rqst["postterm"] then
    for index,termcmd in ipairs(rqst["postterm"]) do
        cmd.exec(termcmd)
    end
end
