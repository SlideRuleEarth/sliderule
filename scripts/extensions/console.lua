local console_monitor = core.monitor(core.LOG, core.INFO, core.FMT_TEXT):name("console.monitor")
console_monitor:tail(1024)

local console_dispatcher = core.dispatcher(core.EVENTQ):name("console.dispatcher")
console_dispatcher:attach(console_monitor, "eventrec")
console_dispatcher:run()

local function loglvl (lvl)
    console_monitor:config(core.LOG, lvl)
    sys.setlvl(core.LOG, lvl)
end

local package = {
    dispatcher = console_dispatcher,
    monitor = console_monitor,
    loglvl = loglvl
}

return package
