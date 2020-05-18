--
-- Purpose:     User interface for sigview application
--
-- Parameters:  <json input>
--
-- Examples:    sliderule sigview txstraddle.json
--
-- Notes:       see sigview.md for details on how to setup json file
--

-- Paths and Packages --
package.path = package.path .. ";./?.lua" -- put local scripts in require search path
console = require("console")
metric = require("metric")
packet = require("packet")
json = require("json")
cfg = require("sigcfg")

-- Command Line Parameter Function --
local function clp (parm)
    argval = parm
    if type(parm) == "string" then
        if string.sub(parm, 1, 1) == "$" then
            argnum = string.sub(parm, 2, -1)
            argval = arg[tonumber(argnum) + 1]
        end
    end
    return argval
end

-- Process Command Line Argument: JSON Data Description File
json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    cmd.log("CRITICAL", string.format('Opening up JSON file: %s\n', json_input))
    cmd.exec("START_STOPWATCH")
else
    cmd.log("CRITICAL", "Missing json file, exiting...\n")
    cmd.exec("ABORT")
end

-- Read JSON File and Decode Content --
local f = io.open(json_input, "r")
local content = f:read("*all")
f:close()
local rectable = json.decode(content)

-- Read in configuration parameters --
local threads = 4
local timeout_seconds = 3
local gui = clp(rectable["gui"])
local block_on_complete = clp(rectable["block"])
if rectable["threads"] then threads = clp(rectable["threads"]) end
if rectable["timeout"] then timeout_seconds = clp(rectable["timeout"]) end
local database = clp(rectable["database"])

-- Configure metric utility --
metric.threads = threads

-- Set Logging Level --
console.logger:config(core.ERROR)

-- Initialize Sigview --
cfg.setQDepth()
cfg.startPacketProcessor(threads)
cfg.startLogs()     
cfg.startDatabase(database)
cfg.startEchoes()     
cfg.startScienceProcessing() 
--cfg.startLaser()         
--cfg.startBce()         
--cfg.startReport()         
cfg.startPacketParsers()     

-- Create Report Dispatcher: Output File Name  --    
outfilename = "metric.csv"
s1,s2 = string.find(json_input, ".json")
if s1 then
    outfilename = string.format('%s.csv', string.sub(json_input, 0, s1 - 1))
end

console.logger:config(core.INFO)

-- Create Metrics --
if rectable["metric"] then
    reportname = metric.createMetrics(rectable["metric"], outfilename)
end

-- Create Limits --
if rectable["limit"] then
    metric.createLimits(rectable["limit"])
end

-- Establish Data Source --
local source = clp(rectable["source"])
if source == nil then
    cmd.log("CRITICAL", "No data source specified\n");
elseif clp(source["type"]) == "DATASRV" then
    -- Setup Datasrv Parameters --
    local datasrv_local_ip = "192.168.216.11"
    local datasrv_remote_ip = "128.183.130.20"
    local datasrv_ip = datasrv_local_ip
    local datasrv_arch = "SPW_SSR_ARCH"
    if clp(source["location"]) == "REMOTE" then datasrv_ip = datasrv_remote_ip end
    if source["arch"] then datasrv_arch = clp(source["arch"]) end
    local start_time = clp(source["start"])
    local stop_time = clp(source["stop"])
    -- Connect to Datasrv --
    if start_time == nil or stop_time == nil then
        -- Real-Time Stream --
        datasrvCmdSock = core.tcp(datasrv_ip, 33102, core.CLIENT)
        datasrvCmdWriter = core.writer(datasrvCmdSock, "datasrv_cmdq")
        datasrvTlmSock = core.tcp(datasrv_ip, 35505, core.CLIENT)
        datasrvTlmReader = core.reader(datasrvTlmSock, "ccsdsdataq")
        -- Send Command to Datasrv to Enable SSR Telemetry --
        datasrv_cmdq = msg.publish("datasrv_cmdq")
        cmd.exec("CCSDS::DEFINE_COMMAND datasrv.cmd NULL 0x06D4 1 8 2")
        cmd.exec("ADD_FIELD datasrv.cmd CS UINT8 7 1 LE")
        ssrcmd = msg.create("/datasrv.cmd")
        raw = ssrcmd:serialize()
        cs = packet.computeChecksum(raw)
        ssrcmd:setvalue("CS", cs)
        datasrv_cmdq:sendrecord(ssrcmd)
        io.write("Enable SSR data, sending command: ")
        raw = ssrcmd:serialize()
        packet.printPacket(raw)
    else
        -- Playback Stream --
        cmd.exec(string.format('NEW DATASRV_READER datasrv %s 36606 ccsdsdataq %s %s %s', datasrv_ip, start_time, stop_time, datasrv_arch))
        -- Wait for playback to finish --
        if block_on_complete then
            cmd.exec("WAIT 1")
            cmd.stopuntil("datasrv", false, 0)
        end
    end
elseif clp(source["type"]) == "FILE" then
    -- Set Data Input Q (based on format) --
    local parseq = "ccsdsdataq"
    if      clp(source["format"]) == "CCSDS" then parseq = "ccsdsdataq"
    elseif  clp(source["format"]) == "ITOS"  then parseq = "itosdataq"
    elseif  clp(source["format"]) == "MOC"   then parseq = "mocdataq"
    elseif  clp(source["format"]) == "SPW"   then parseq = "spwdataq"
    elseif  clp(source["format"]) == "ADAS"  then parseq = "adasdataq"
    elseif  clp(source["format"]) == "SIS"   then parseq = "sisdataq"
    elseif  clp(source["format"]) == "SSR"   then parseq = "ssrdataq"
    elseif  clp(source["format"]) == "CDH"   then parseq = "cdhdataq" end	
    -- Create File Reader --
    local filepaths = clp(source["files"])
    sourceFile = core.file(core.READER, core.BINARY, filepaths)
    sourceFileReader = core.reader(sourceFile, parseq)
    -- Wait for File Reader to Finish Reading File --
    if block_on_complete then
        cmd.exec("WAIT 1")
        cmd.stopuntil("sourceFile", false, 0)
    end
end

-- Wait for Data to Finish --
if block_on_complete then
    cmd.exec(string.format('WAIT_ON_EMPTY scidataq %d', timeout_seconds), -1) -- wait forever
    cmd.exec("WAIT_ON_EMPTY metricq 3", -1) -- wait forever
    if reportname then
        cmd.exec(string.format('%s::FLUSH_ROW ALL', reportname))
    end
    cmd.exec("DISPLAY_STOPWATCH")
    cmd.exec("ABORT")
end

