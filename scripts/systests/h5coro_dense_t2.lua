-- H5CORO DENSE TEST 2 --
-- Variable: geospatial_lat_lon_extent
-- Attributes: 
    -- geospatial_eastbound_longitude (32 float pt)
    -- geospatial_lat_center (32 float pt)
    -- geospatial_lon_center (32 float pt)
    -- geospatial_lat_units (string, dynamic size)
    -- geospatial_lon_nadir (32 float pt)

    local runner = require("test_executive")
    local console = require("console")
    local td = runner.rootdir(arg[0])
    
    -- Setup --
    console.monitor:config(core.LOG, core.DEBUG) 
    sys.setlvl(core.LOG, core.DEBUG)
    asset = core.asset("local", "nil", "file", td, "empty.index")
    
    -- Unit Test --
    print('\n------------------\nTest: Read Dataset\n------------------')
    
    dataq = "dataq"
    rsps2 = msg.subscribe(dataq)
    
    -- Arguments & Call --
    
    -- SUB TEST 1 - eastbound_longitude -- 
    
    local resource_path = "OR_ABI-L2-FDCC-M3_G17_s20182390052191_e20182390054564_c20182390055159.nc"
    local dataset_name = "/geospatial_lat_lon_extent/geospatial_eastbound_longitude"
    local id = 0
    local raw = true
    local dtype = core.DYNAMIC
    local col = 0
    local startrow = 0
    local numrows = core.ALL_ROWS
    
    f2 = h5.dataset(core.READER, 
                    asset, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r2 = core.reader(f2, dataq)
    vals = rsps2:recvstring(3000) -- read out from listner
    e1 = string.unpack('f', vals)
    print("RESULT geospatial_eastbound_longitude: " .. e1)
    print("NOTE/TODO: precison is higher for h5coro - HDF5 src is up to -49.17929 only")
    runner.check("-49.179290771484" == tostring(e1), "failed dataset read")

    rsps2:destroy()
    r2:destroy()
    
    -- SUB TEST 2 - scale_factor -- 
    
    asset2 = core.asset("local", "nil", "file", td, "empty.index")
    dataq = "dataq2"
    rsps2s2 = msg.subscribe(dataq)
    
    dataset_name = "/geospatial_lat_lon_extent/geospatial_lat_center"
    dtype = core.DYNAMIC
    
    f3 = h5.dataset(core.READER, 
                    asset2, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r3 = core.reader(f3, dataq)
    vals = rsps2s2:recvstring(3000) -- read out from listner
    e1 = string.unpack('f', vals)
    print("RESULT geospatial_lat_center: " .. e1)
    print("NOTE/TODO: precison is higher for h5coro - HDF5 src is up to 29.294 only")
    runner.check("29.29400062561" == tostring(e1), "failed dataset read")
    
    rsps2s2:destroy()
    r3:destroy()
    
    -- SUB TEST 3 - geospatial_lon_center -- 
    
    asset3 = core.asset("local", "nil", "file", td, "empty.index")
    dataq = "dataq3" -- stream based: data comes in, then consumed/produced
    rsps2s3 = msg.subscribe(dataq) -- responses posted to dataq
    
    dataset_name = "/geospatial_lat_lon_extent/geospatial_lon_center"
    dtype = core.DYNAMIC
    
    f3 = h5.dataset(core.READER, 
                    asset3, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r4 = core.reader(f3, dataq)
    vals = rsps2s3:recvstring(3000)
    e1 = string.unpack('f', vals)
    print("RESULT geospatial_lon_center: " .. e1)
    print("NOTE/TODO: precison is higher for h5coro - HDF5 src is up to -91.406 only")
    runner.check("-91.40599822998" == tostring(e1), "failed dataset read")
    
    rsps2s3:destroy()
    r4:destroy()
    
    -- SUB TEST 4 - coordinates -- 
    
    asset4 = core.asset("local", "nil", "file", td, "empty.index")
    dataq = "dataq4"
    rsps2s4 = msg.subscribe(dataq)
    
    dataset_name = "/geospatial_lat_lon_extent/geospatial_lat_units"
    dtype = core.DYNAMIC -- NOTE: TEXT not valid 
    
    f4 = h5.dataset(core.READER, 
                    asset4, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r5 = core.reader(f4, dataq)
    vals = rsps2s4:recvstring(3000)
    print("RESULT geospatial_lat_units (string)" .. " '" .. vals .. "' ")
    runner.check("degrees_north" == vals, "failed dataset read")
    
    rsps2s4:destroy()
    r5:destroy()
    
    -- SUB TEST 5 - geospatial_lon_nadir --
    
    asset5 = core.asset("local", "nil", "file", td, "empty.index")
    dataq = "dataq5"
    rsps2s5 = msg.subscribe(dataq)
    
    dataset_name = "/geospatial_lat_lon_extent/geospatial_lon_nadir"
    dtype = core.DYNAMIC
    
    f5 = h5.dataset(core.READER, 
                    asset5, 
                    resource_path, 
                    dataset_name, 
                    id, 
                    raw, 
                    dtype, 
                    col, 
                    startrow, 
                    numrows)
    
    r6 = core.reader(f5, dataq)
    vals = rsps2s5:recvstring(3000) -- read out from listner
    e1 = string.unpack('f', vals)
    print("RESULT geospatial_lon_nadir: " .. e1)
    runner.check("-89.5" == tostring(e1), "failed dataset read")
    
    rsps2s5:destroy()
    r6:destroy()
    
   -- Report Results --
    
    runner.report()