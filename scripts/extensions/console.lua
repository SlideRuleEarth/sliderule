local console_monitor = core.monitor(core.LOG, core.INFO, core.FMT_TEXT):tail(1024):name("console.monitor")

local function loglvl (lvl)
    console_monitor:config(core.LOG, lvl)
    sys.setlvl(core.LOG, lvl)
end

local package = {
    monitor = console_monitor,
    loglvl = loglvl
}

return package
