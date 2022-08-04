local console = require("console")
local asset = require("asset")
local json = require("json")

-- Process Arguments: JSON Configuration File
local cfgtbl = {}
local json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.CRITICAL, string.format('Reading json file: %s\n', json_input))
    local f = io.open(json_input, "r")
    if f ~= nil then
        local content = f:read("*all")
        f:close()
        cfgtbl = json.decode(content)
    end
end

-- Pull Out Parameters
local loglvl = cfgtbl["loglvl"] or core.ERROR
local port = cfgtbl["server_port"] or 9082
local rec_path = cfgtbl["rec_path"] or "../../itos/rec/atlas/fsw/*.rec"
local depth = cfgtbl["qdepth"] or 50000
local num_threads = cfgtbl["num_threads"] or 1
local time_stat = cfgtbl["time_stat"] or true
local pkt_stat = cfgtbl["pkt_stat"] or false
local ch_stat = cfgtbl["ch_stat"] or true
local tx_stat = cfgtbl["tx_stat"] or false
local sig_stat = cfgtbl["sig_stat"] or true

-- Set Queue Depth
cmd.exec(string.format('STREAM_QDEPTH %d', depth))

-- Create Database from ITOS Records
cmd.exec("NEW ITOS_RECORD_PARSER itosdb")
cmd.exec(string.format('itosdb::LOAD_REC_FILES %s', rec_path))
cmd.exec("itosdb::SET_DESIGNATIONS  atlas_applicationId atlas_functionCode NULL")
cmd.exec("itosdb::BUILD_DATABASE")
cmd.exec("itosdb::BUILD_RECORDS")

-- Start Packet Processor
cmd.exec(string.format('NEW CCSDS_PACKET_PROCESSOR pktProc %s %d', "scidataq", num_threads))

-- Start Logs
sbcdiaglog  = core.writer(core.file(core.WRITER, core.TEXT, "sbcdiag.log", core.FLUSHED), "sbcdiaglogq"):name("sbcdiaglog")
pce1diaglog = core.writer(core.file(core.WRITER, core.TEXT, "pce1diag.log", core.FLUSHED), "pce1diaglogq"):name("pce1diaglog")
pce2diaglog = core.writer(core.file(core.WRITER, core.TEXT, "pce2diag.log", core.FLUSHED), "pce2diaglogq"):name("pce2diaglog")
pce3diaglog = core.writer(core.file(core.WRITER, core.TEXT, "pce3diag.log", core.FLUSHED), "pce3diaglogq"):name("pce3diaglog")
cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProcSbc sbcdiaglogq NULL")
cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProc1 pce1diaglogq NULL 1")
cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProc2 pce2diaglogq NULL 2")
cmd.exec("NEW DIAG_LOG_PROCESSOR diagLogProc3 pce3diaglogq NULL 3")
cmd.exec("pktProc::REGISTER   0x414 diagLogProcSbc")  -- SBC HS LOG
cmd.exec("pktProc::REGISTER   0x43B diagLogProc1")    -- PCE 1 HS LOG
cmd.exec("pktProc::REGISTER   0x44B diagLogProc2")    -- PCE 2 HS LOG
cmd.exec("pktProc::REGISTER   0x45B diagLogProc3")    -- PCE 3 HS LOG

-- Start Echoes
cmdecho = core.writer(core.file(core.WRITER, core.TEXT, "cmdecho.log", core.FLUSHED), "cmdechoq"):name("cmdecho")
cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProc1 cmdechoq itosdb 1")
cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProc2 cmdechoq itosdb 2")
cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProc3 cmdechoq itosdb 3")
cmd.exec("NEW CMD_ECHO_PROCESSOR cmdEchoProcSbc cmdechoq itosdb")
cmd.exec("pktProc::REGISTER   0x42E cmdEchoProcSbc") -- SBC CMD ECHO
cmd.exec("pktProc::REGISTER   0x43F cmdEchoProc1")   -- PCE 1 CMD ECHO
cmd.exec("pktProc::REGISTER   0x44F cmdEchoProc2")   -- PCE 2 CMD ECHO
cmd.exec("pktProc::REGISTER   0x45F cmdEchoProc3")   -- PCE 3 CMD ECHO

-- Start Major Frame Processors
cmd.exec("NEW MAJOR_FRAME_PROCESSOR mfProc1")
cmd.exec("NEW MAJOR_FRAME_PROCESSOR mfProc2")
cmd.exec("NEW MAJOR_FRAME_PROCESSOR mfProc3")
cmd.exec("pktProc::REGISTER   0x430 mfProc1")
cmd.exec("pktProc::REGISTER   0x440 mfProc2")
cmd.exec("pktProc::REGISTER   0x450 mfProc3")

-- Start Time Processor
cmd.exec("NEW TIME_PROCESSOR timeProc")
cmd.exec("pktProc::REGISTER   0x402 timeProc")  -- SIM_HK
cmd.exec("pktProc::REGISTER   0x409 timeProc")  -- SXP_HK
cmd.exec("pktProc::REGISTER   0x486 timeProc")  -- SXP_DIAG
cmd.exec("pktProc::REGISTER   0x473 timeProc")  -- PCE 1 TIMEKEEPING
cmd.exec("pktProc::REGISTER   0x474 timeProc")  -- PCE 2 TIMEKEEPING
cmd.exec("pktProc::REGISTER   0x475 timeProc")  -- PCE 3 TIMEKEEPING

-- Start Time Tag Processors --
cmd.exec(string.format("NEW TIME_TAG_PROCESSOR ttProc1 %s 1", "recdataq"))
cmd.exec(string.format("NEW TIME_TAG_PROCESSOR ttProc2 %s 2", "recdataq"))
cmd.exec(string.format("NEW TIME_TAG_PROCESSOR ttProc3 %s 3", "recdataq"))
cmd.exec("ttProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
cmd.exec("ttProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
cmd.exec("ttProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
cmd.exec("ttProc1::ATTACH_TIME_PROC timeProc")
cmd.exec("ttProc2::ATTACH_TIME_PROC timeProc")
cmd.exec("ttProc3::ATTACH_TIME_PROC timeProc")
cmd.exec("pktProc::REGISTER   0x4E6 ttProc1")
cmd.exec("pktProc::REGISTER   0x4F0 ttProc2")
cmd.exec("pktProc::REGISTER   0x4FA ttProc3")

-- Start Strong Altimetric Processors
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR salProc1 SAL %s 1", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR salProc2 SAL %s 2", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR salProc3 SAL %s 3", "recdataq"))
cmd.exec("salProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
cmd.exec("salProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
cmd.exec("salProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
cmd.exec("pktProc::REGISTER   0x4E2 salProc1")
cmd.exec("pktProc::REGISTER   0x4EC salProc2")
cmd.exec("pktProc::REGISTER   0x4F6 salProc3")

-- Start Weak Altimetric Processors
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR walProc1 WAL %s 1", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR walProc2 WAL %s 2", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR walProc3 WAL %s 3", "recdataq"))
cmd.exec("walProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
cmd.exec("walProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
cmd.exec("walProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
cmd.exec("pktProc::REGISTER   0x4E3 walProc1")
cmd.exec("pktProc::REGISTER   0x4ED walProc2")
cmd.exec("pktProc::REGISTER   0x4F7 walProc3")

-- Start Strong Atmospheric Processors
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR samProc1 SAM %s 1", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR samProc2 SAM %s 2", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR samProc3 SAM %s 3", "recdataq"))
cmd.exec("samProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
cmd.exec("samProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
cmd.exec("samProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
cmd.exec("pktProc::REGISTER   0x4E4 samProc1")
cmd.exec("pktProc::REGISTER   0x4EE samProc2")
cmd.exec("pktProc::REGISTER   0x4F8 samProc3")

-- Start Weak Atmospheric Processors
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR wamProc1 WAM %s 1", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR wamProc2 WAM %s 2", "recdataq"))
cmd.exec(string.format("NEW ALTIMETRY_PROCESSOR wamProc3 WAM %s 3", "recdataq"))
cmd.exec("wamProc1::ATTACH_MAJOR_FRAME_PROC mfProc1")
cmd.exec("wamProc2::ATTACH_MAJOR_FRAME_PROC mfProc2")
cmd.exec("wamProc3::ATTACH_MAJOR_FRAME_PROC mfProc3")
cmd.exec("pktProc::REGISTER   0x4E5 wamProc1")
cmd.exec("pktProc::REGISTER   0x4EF wamProc2")
cmd.exec("pktProc::REGISTER   0x4F9 wamProc3")

-- Attach Statistics
if time_stat then
    cmd.exec(string.format("timeProc.TimeStat::ATTACH %s", "recdataq"))
end
if pkt_stat then
    cmd.exec(string.format("ttProc1.PktStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc2.PktStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc3.PktStat::ATTACH %s", "recdataq"))
end
if ch_stat then
    cmd.exec(string.format("ttProc1.ChStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc2.ChStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc3.ChStat::ATTACH %s", "recdataq"))
end
if tx_stat then
    cmd.exec(string.format("ttProc1.TxStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc2.TxStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc3.TxStat::ATTACH %s", "recdataq"))
end
if sig_stat then
    cmd.exec(string.format("ttProc1.SigStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc2.SigStat::ATTACH %s", "recdataq"))
    cmd.exec(string.format("ttProc3.SigStat::ATTACH %s", "recdataq"))
end

-- Start Laser --
cmd.exec("NEW LASER_PROCESSOR laserProc")
cmd.exec("pktProc::REGISTER   0x425 laserProc") -- HKT_C (temperatures)
cmd.exec("pktProc::REGISTER   0x427 laserProc") -- HKT_E (laser energies)

-- Configure Logging
sys.setlvl(core.LOG, loglvl)

-- Configure and Run Server --
server = core.httpd(port,nil,2):name("HttpServer")
endpoint = core.endpoint():name("LuaEndpoint")
server:attach(endpoint, "/source")
