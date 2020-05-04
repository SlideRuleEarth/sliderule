local pkg = {}

-----------------------------
-- Confgiration State
-----------------------------
pkg.proc_started    = false
pkg.logs_started    = false
pkg.db_started      = false
pkg.echoe_started   = false
pkg.sci_started     = false
pkg.laser_started   = false
pkg.bce_started     = false
pkg.report_started  = false
pkg.parse_started   = false
pkg.gui_started     = false

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
    if pkg.proc_started then return true end
    pkg.proc_started = true
    local num_threads = _num_threads or 4
    cmd.exec(string.format('NEW CCSDS_PACKET_PROCESSOR pktProc scidataq %d', num_threads))
    return true
end

-----------------------------
-- Start Logs
-----------------------------
local function startLogs ()
    if pkg.logs_started then return true end
    if not pkg.proc_started then return false end
    pkg.logs_started = true
    cmd.exec("NEW DEVICE_WRITER sbcdiaglog FILE TEXT sbcdiag.log FLUSHED sbcdiaglogq")
    cmd.exec("NEW DEVICE_WRITER pce1diaglog FILE TEXT pce1diag.log FLUSHED pce1diaglogq")
    cmd.exec("NEW DEVICE_WRITER pce2diaglog FILE TEXT pce2diag.log FLUSHED pce2diaglogq")
    cmd.exec("NEW DEVICE_WRITER pce3diaglog FILE TEXT pce3diag.log FLUSHED pce3diaglogq")
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
    if pkg.db_started then return true end
    pkg.db_started = true
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
    if pkg.echoe_started then return true end
    if not pkg.db_started then return false end
    if not pkg.proc_started then return false end
    echoe_started = true
    cmd.exec("NEW DEVICE_WRITER cmdecho FILE TEXT cmdecho.log FLUSHED cmdechoq")
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
    if pkg.sci_started then return true end
    if not pkg.proc_started then return false end
    pkg.sci_started = true
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
    cmd.exec("NEW TIME_TAG_PROCESSOR ttProc1 recdataq txtimeq 1")
    cmd.exec("NEW TIME_TAG_PROCESSOR ttProc2 recdataq txtimeq 2")
    cmd.exec("NEW TIME_TAG_PROCESSOR ttProc3 recdataq txtimeq 3")
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
    cmd.exec("NEW ALTIMETRY_PROCESSOR salProc1 SAL recdataq 1") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR salProc2 SAL recdataq 2")
    cmd.exec("NEW ALTIMETRY_PROCESSOR salProc3 SAL recdataq 3")  
    cmd.exec("salProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("salProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("salProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E2 salProc1")
    cmd.exec("pktProc::REGISTER   0x4EC salProc2")
    cmd.exec("pktProc::REGISTER   0x4F6 salProc3")
    -- start weak altimetric processors --
    cmd.exec("NEW ALTIMETRY_PROCESSOR walProc1 WAL recdataq 1") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR walProc2 WAL recdataq 2") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR walProc3 WAL recdataq 3") 
    cmd.exec("walProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("walProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("walProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E3 walProc1")
    cmd.exec("pktProc::REGISTER   0x4ED walProc2")
    cmd.exec("pktProc::REGISTER   0x4F7 walProc3")
    -- start strong atmospheric processors --
    cmd.exec("NEW ALTIMETRY_PROCESSOR samProc1 SAM recdataq 1") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR samProc2 SAM recdataq 2") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR samProc3 SAM recdataq 3") 
    cmd.exec("samProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("samProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("samProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E4 samProc1")
    cmd.exec("pktProc::REGISTER   0x4EE samProc2")
    cmd.exec("pktProc::REGISTER   0x4F8 samProc3")
    -- start weak atmospheric processors -- 
    cmd.exec("NEW ALTIMETRY_PROCESSOR wamProc1 WAM recdataq 1") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR wamProc2 WAM recdataq 2") 
    cmd.exec("NEW ALTIMETRY_PROCESSOR wamProc3 WAM recdataq 3") 
    cmd.exec("wamProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
    cmd.exec("wamProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
    cmd.exec("wamProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
    cmd.exec("pktProc::REGISTER   0x4E5 wamProc1")
    cmd.exec("pktProc::REGISTER   0x4EF wamProc2")
    cmd.exec("pktProc::REGISTER   0x4F9 wamProc3")
    -- attach statistics --
    if enTimeStat then 
        cmd.exec("timeProc.TimeStat::ATTACH recdataq") 
    end
    if enPktStat then 
        cmd.exec("ttProc1.PktStat::ATTACH recdataq")
        cmd.exec("ttProc2.PktStat::ATTACH recdataq")
        cmd.exec("ttProc3.PktStat::ATTACH recdataq")
    end
    if enChStat then 
        cmd.exec("ttProc1.ChStat::ATTACH recdataq")
        cmd.exec("ttProc2.ChStat::ATTACH recdataq")
        cmd.exec("ttProc3.ChStat::ATTACH recdataq")
    end
    if enTxStat then
        cmd.exec("ttProc1.TxStat::ATTACH recdataq")
        cmd.exec("ttProc2.TxStat::ATTACH recdataq")
        cmd.exec("ttProc3.TxStat::ATTACH recdataq")
    end
    if enSigStat then
        cmd.exec("ttProc1.SigStat::ATTACH recdataq")
        cmd.exec("ttProc2.SigStat::ATTACH recdataq")
        cmd.exec("ttProc3.SigStat::ATTACH recdataq")
    end
    -- return success -- 
    return true
end

-----------------------------
-- Start Laser
-----------------------------
local function startLaser ()
    if pkg.laser_started then return true end
    if not pkg.proc_started then return false end
    pkg.laser_started = true
    cmd.exec("NEW LASER_PROCESSOR laserProc")
    cmd.exec("pktProc::REGISTER   0x425 laserProc") -- HKT_C (temperatures)
    cmd.exec("pktProc::REGISTER   0x427 laserProc") -- HKT_E (laser energies)
    return true
end

-----------------------------
-- Start BCE
-----------------------------
local function startBce ()
    if pkg.bce_started then return true end
    if not pkg.proc_started then return false end
    pkg.bce_started = true
    cmd.exec("NEW BCE_PROCESSOR bceProc recdataq")
    cmd.exec("pktProc::REGISTER   0x605 bceProc")
    cmd.exec("pktProc::REGISTER   0x607 bceProc")
    cmd.exec("pktProc::REGISTER   0x60F bceProc")
    cmd.exec("pktProc::REGISTER   0x610 bceProc")
    cmd.exec("pktProc::REGISTER   0x611 bceProc")
    cmd.exec("pktProc::REGISTER   0x612 bceProc")
    cmd.exec("pktProc::REGISTER   0x613 bceProc")
    cmd.exec("pktProc::REGISTER   0x614 bceProc")
    cmd.exec("pktProc::REGISTER   0x615 bceProc")
    cmd.exec("pktProc::REGISTER   0x616 bceProc")
    cmd.exec("pktProc::REGISTER   0x617 bceProc")
    cmd.exec("pktProc::REGISTER   0x618 bceProc")
    cmd.exec("pktProc::REGISTER   0x619 bceProc")
    cmd.exec("pktProc::REGISTER   0x61A bceProc")
    cmd.exec("pktProc::REGISTER   0x61B bceProc")
    cmd.exec("pktProc::REGISTER   0x61C bceProc")
    cmd.exec("pktProc::REGISTER   0x61D bceProc")
    cmd.exec("pktProc::REGISTER   0x61E bceProc")
    cmd.exec("pktProc::REGISTER   0x61F bceProc")
    return true
end

-----------------------------
-- Start Report
-----------------------------
local function startReport ()
    if pkg.report_started then return true end
    if not pkg.sci_started then return false end
    if not pkg.laser_started then return false end
    if not pkg.bce_started then return false end
    pkg.report_started = true    
    cmd.exec("NEW REPORT_STATISTIC reportStat ttProc1 ttProc2 ttProc3 timeProc bceProc laserProc")
    cmd.exec("reportStat::ATTACH recdataq")
    return true
end

-----------------------------
-- Start Packet Parsers
-----------------------------
local function startPacketParsers ()
    if pkg.parser_started then return true end
    pkg.parser_started = true
    -- create parser modules --
    cmd.exec("NEW CCSDS_PARSER_MODULE     ccsdsParserModule")
    cmd.exec("NEW STRIP_PARSER_MODULE     itosParserModule 35")
    cmd.exec("NEW STRIP_PARSER_MODULE     mocParserModule 12")
    cmd.exec("NEW STRIP_PARSER_MODULE     spwParserModule 2")
    cmd.exec("NEW ZFRAME_PARSER_MODULE    adasParserModule FALSE")
    cmd.exec("NEW ZFRAME_PARSER_MODULE    sisParserModule TRUE")
    cmd.exec("NEW AOSFRAME_PARSER_MODULE  ssrParserModule 206 6 224 NONE 0 1104 6 2")
    cmd.exec("NEW AOSFRAME_PARSER_MODULE  cdhParserModule 206 1 224 NONE 0 1115 15 4")
    -- create packet parsers --
    cmd.exec("NEW CCSDS_PACKET_PARSER ccsdsParser ccsdsParserModule   SPACE ccsdsdataq    scidataq NULL") -- binary stream of CCSDS packets
    cmd.exec("NEW CCSDS_PACKET_PARSER itosParser  itosParserModule    SPACE itosdataq     scidataq NULL") -- ITOS archive file format
    cmd.exec("NEW CCSDS_PACKET_PARSER mocParser   mocParserModule     SPACE mocdataq      scidataq NULL") -- ITOS archive file format as used at the MOC
    cmd.exec("NEW CCSDS_PACKET_PARSER spwParser   spwParserModule     SPACE spwdataq      scidataq NULL") -- binary stream of SpaceWire packets
    cmd.exec("NEW CCSDS_PACKET_PARSER adasParser  adasParserModule    SPACE adasdataq     scidataq NULL") -- ADAS real-time data stream (socket)
    cmd.exec("NEW CCSDS_PACKET_PARSER sisParser   sisParserModule     SPACE sisdataq      scidataq NULL") -- ADAS raw data from SIS (file)
    cmd.exec("NEW CCSDS_PACKET_PARSER ssrParser   ssrParserModule     SPACE ssrdataq      scidataq NULL") -- ADAS raw data from SSR (file)
    cmd.exec("NEW CCSDS_PACKET_PARSER cdhParser   cdhParserModule     SPACE cdhdataq      scidataq NULL") -- ADAS raw data from C&DH (file)
    -- filter for real-time adas parser --
    cmd.exec("adasParser::FILTER ENABLE ALL")
    cmd.exec("adasParser::FILTER DISABLE 0x64B") --NTGSE packet that does not have secondary header polluting instrument telemetry stream
    cmd.exec("adasParser::FILTER DISABLE RANGE 0x4BF 0x4E1") -- ESIM packets
    cmd.exec("adasParser::FILTER DISABLE RANGE 0x492 0x498") -- ESIM packets
    -- filter for SIS adas file parser --
    cmd.exec("sisParser::FILTER ENABLE ALL")
    cmd.exec("sisParser::FILTER DISABLE 0x64B") -- NTGSE packet that does not have secondary header polluting instrument telemetry stream
    cmd.exec("sisParser::FILTER DISABLE RANGE 0x4BF 0x4E1") -- ESIM packets
    cmd.exec("sisParser::FILTER DISABLE RANGE 0x492 0x498") -- ESIM packets
    -- return success -- 
    return true
end

-----------------------------
-- Start GUI
-----------------------------
local function startGui ()
    if pkg.gui_started then return true end
    if not pkg.sci_started then return false end
    if not pkg.report_started then return false end
    if not pkg.proc_started then return false end
    pkg.gui_started = true
    cmd.exec("NEW SHELL shell mlogq")
    cmd.exec("NEW VIEWER viewer recdataq scidataq ttProc1 ttProc2 ttProc3 reportStat timeProc pktProc")
    --                               ADASFILE      ASCII      BINARY     SIS      ITOSARCH  ADAS      NTGSE    DATASRV    AOS
    cmd.exec("viewer::SET_PARSERS adasfiledataq ccsdsdataq ccsdsdataq spwdataq itosdataq adasdataq spwdataq ccsdsdataq aosfiledataq")
    cmd.exec("viewer::SET_PLAY_RATE       50.0")
    cmd.exec("viewer::SET_DATA_MODE       SAMPLE")
    cmd.exec("viewer::SET_PLOT_BUF_SIZE   -1")
    cmd.exec("viewer::SET_PLOT_EMPTY      TRUE")
    return true
end

-----------------------------
-- Configure Lab
-----------------------------
local function configureLab ()
    if not setQDepth()              then return false end
    if not startPacketProcessor()   then return false end
    if not startLogs()              then return false end
    if not startDatabase()          then return false end
    if not startEchoes()            then return false end
    if not startScienceProcessing() then return false end
    if not startLaser()             then return false end
    if not startBce()               then return false end
    if not startReport()            then return false end
    if not startPacketParsers()     then return false end
    if not startGui()               then return false end
    return true
end

-----------------------------
-- Set Package Methos
-----------------------------
pkg.setQDepth               = setQDepth
pkg.startPacketProcessor    = startPacketProcessor
pkg.startLogs               = startLogs
pkg.startDatabase           = startDatabase
pkg.startEchoes             = startEchoes
pkg.startScienceProcessing  = startScienceProcessing
pkg.startLaser              = startLaser
pkg.startBce                = startBce
pkg.startReport             = startReport
pkg.startPacketParsers      = startPacketParsers
pkg.startGui                = startGui
pkg.configureLab            = configureLab

-----------------------------
-- Return Package
-----------------------------
return pkg
