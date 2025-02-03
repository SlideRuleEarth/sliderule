--
-- ENDPOINT:    /source/xdem
--
local json      = require("json")
local runner    = require("container_runtime")
local rqst      = json.decode(arg[1])
local timeout   = rqst["rqst_timeout"] or 600000

-------------------------------------------------------
-- initialize container runtime environment
-------------------------------------------------------
local crenv = runner.setup()
if not crenv.host_sandbox_directory then
    return json.dumps({message = "error initializing container runtime environment"})
end

-------------------------------------------------------
-- run xdem container
-------------------------------------------------------
local settings = {
    fn_epc = rqst["fn_epc"] or "test.parquet",
    fn_dem = rqst["fn_dem"] or "test.dem",
    fn_reg = string.format("%s/coregistration_results.json", crenv.container_sandbox_mount)
}
local container_parms = {
    container_image = "xdem",
    container_name = "xdem",
    container_command = string.format("/env/bin/python /runner.py %s/settings.json", crenv.container_sandbox_mount),
    timeout = timeout
}
local container = runner.execute(crenv, container_parms, { ["settings.json"] = settings }, rspq)
runner.wait(container, timeout)
runner.cleanup(crenv)

-------------------------------------------------------
-- return results
-------------------------------------------------------
local f, err = io.open(string.format("%s/coregistration_results.json", crenv.host_sandbox_directory), "r")
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

