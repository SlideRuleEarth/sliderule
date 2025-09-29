--
-- ENDPOINT:    /source/xdem
--
local json      = require("json")
local runner    = require("container_runtime")
local rqst      = json.decode(arg[1])
local timeout   = rqst["rqst_timeout"] or 600000
local parms     = rqst["parms"]
local image     = rqst["image"]
local command   = rqst["command"]
local input     = "parms.json" -- well known file name
local output    = "results.json" -- well known file name

-------------------------------------------------------
-- check if private cluster
-------------------------------------------------------
if sys.getcfg("is_public") then
    return
end

-------------------------------------------------------
-- initialize container runtime environment
-------------------------------------------------------
local crenv = runner.setup()
if not crenv.host_sandbox_directory then
    return json.dumps({message = "error initializing container runtime environment"})
end

-------------------------------------------------------
-- run container
-------------------------------------------------------
local container_parms = {
    container_image = image,
    container_name = "cre-".._rqst.id,
    container_command = string.format("%s %s/%s %s/%s", command, crenv.container_sandbox_mount, input, crenv.container_sandbox_mount, output),
    timeout = timeout
}
local container = runner.execute(crenv, container_parms, { [input] = parms }, _rqst.rspq)
runner.wait(container, timeout)
runner.cleanup(crenv)

-------------------------------------------------------
-- return results
-------------------------------------------------------
local f, err = io.open(string.format("%s/%s", crenv.host_sandbox_directory, output), "r")
if f then
    local data = f:read()
    f:close()
    local status, results = pcall(json.decode, data)
    if status then
        return results
    else
        return { message = string.format("error decoding json: %s", results)}
    end
else
    return { message = string.format("error opening result file: %s", err) }
end