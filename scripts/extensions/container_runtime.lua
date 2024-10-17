local json  = require("json")

--
-- Setup Container Runtime Environment
--
local function setup ()

    local host_sandbox_directory = string.format("%s/%s", cre.HOST_DIRECTORY, rqstid)
    if not cre.createunique(host_sandbox_directory) then
        sys.log(core.WARNING, "Overwriting existing unique shared directory "..host_sandbox_directory)
    end
    return {host_sandbox_directory=host_sandbox_directory, container_sandbox_mount=cre.SANDBOX_MOUNT}
end

--
-- Execute Container
--
local function execute (crenv, parms, settings, response_queue)

    -- write input parameter files
    if settings then
        for filename,contents in pairs(settings) do
            local unique_filename = string.format("%s/%s", crenv.host_sandbox_directory, filename)
            local input_file, msg = io.open(unique_filename, "w")
            if not input_file then
                sys.log(core.CRITICAL, string.format("Failed to create input file %s: %s", unique_filename, msg))
                return false
            end
            input_file:write(json.encode(contents))
            input_file:close()
        end
    end

    -- create container runner (automatically executes container)
    local cre_parms = cre.parms(parms)
    local cre_runner = cre.container(cre_parms, crenv.host_sandbox_directory, response_queue)

    -- return container runner
    return cre_runner

end

--
-- Wait for Container
--
local function wait (cre_runner, timeout)

    return cre_runner:waiton(timeout * 1000)

end

--
-- Cleanup Container Runtime Environment
--
local function cleanup (crenv)

    cre.deleteunique(crenv.host_sandbox_directory)

end

--
-- Package
--
return {
    setup = setup,
    execute = execute,
    wait = wait,
    cleanup = cleanup
}