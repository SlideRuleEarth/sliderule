--
-- Provides functions to query CMR
--
--  NOTES:  The code below uses libcurl (via the netsvc package) to
--          issue a requests

local json = require("json")
local prettyprint = require("prettyprint")

--
-- Constants
--

-- default maximum number of resources to process in one request
DEFAULT_MAX_REQUESTED_RESOURCES = 300

-- best effort match of datasets to providers and versions for earthdata
DATASETS = {
    ATL03 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL06 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL08 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL09 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI01_B =                                            {provider = "LPDAAC_ECS",  version = "002",  api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI02_A =                                            {provider = "LPDAAC_ECS",  version = "002",  api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI02_B =                                            {provider = "LPDAAC_ECS",  version = "002",  api = "cmr",   formats = {".tiff"},  collections = {}},
    GEDI_L3_LandSurface_Metrics_V2_1952 =                 {provider = "ORNL_CLOUD",  version = nil,    api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI_L4A_AGB_Density_V2_1_2056 =                      {provider = "ORNL_CLOUD",  version = nil,    api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI_L4B_Gridded_Biomass_2017 =                       {provider = "ORNL_CLOUD",  version = nil,    api = "cmr",   formats = {".tiff"},  collections = {}},
    HLS =                                                 {provider = "LPCLOUD",     version = nil,    api = "stac",  formats = {".tiff"},  collections = {"HLSS30.v2.0", "HLSL30.v2.0"}},
    ["Digital Elevation Model (DEM) 1 meter"] =           {provider = "USGS",        version = nil,    api = "tnm",   formats = {".tiff"},  collections = {}},
    SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1 =  {provider = "POCLOUD",     version = nil,    api = "cmr",   formats = {".nc"},    collections = {"C2147947806-POCLOUD"}},
    SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1 =        {provider = "POCLOUD",     version = nil,    api = "cmr",   formats = {".nc"},    collections = {"C2152046451-POCLOUD"}}
}

-- best effort match of sliderule assets to earthdata datasets
ASSETS_TO_DATASETS = {
    ["gedil4a"] = "GEDI_L4A_AGB_Density_V2_1_2056",
    ["gedil4b"] = "GEDI_L4B_Gridded_Biomass_2017",
    ["gedil3-elevation"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-canopy"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-elevation-stddev"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-canopy-stddev"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-counts"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil2a"] = "GEDI02_A",
    ["gedil1b"] = "GEDI02_B",
    ["swot-sim-ecco-llc4320"] = "SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1",
    ["swot-sim-glorys"] = "SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1",
    ["usgs3dep-1meter-dem"] = "Digital Elevation Model (DEM) 1 meter",
    ["landsat-hls"] = "HLS",
    ["icesat2"] = "ATL03",
    ["icesat2-atl06"] = "ATL06",
    ["icesat2-atl08"] = "ATL08",
    ["icesat2-atl09"] = "ATL09",
    ["atlas-local"] = "ATL03",
    ["nsidc-s3"] = "ATL03"
}

-- upper limit on resources returned from CMR query per request
CMR_PAGE_SIZE = 2000

-- upper limit on resources returned from STAC query per request
STAC_PAGE_SIZE = 100

-- upper limit on resources returned from TNM query per request
TNM_PAGE_SIZE = 100

--
-- Build GeoJSON
--
local function build_geojson(rsps)
    local geotable = json.decode(rsps) 
    if geotable["links"]                    then geotable["links"] = nil end
    if geotable["numberMatched"]            then geotable["numberMatched"] = nil end
    if geotable["numberReturned"]           then geotable["numberReturned"] = nil end
    if geotable["features"] then
        for i = 1, #geotable["features"] do
            local feature = geotable["features"][i]
            if feature["links"]             then feature["links"] = nil end
            if feature["stac_version"]      then feature["stac_version"] = nil end
            if feature["stac_extensions"]   then feature["stac_extensions"] = nil end
            if feature["collection"]        then feature["collection"] = nil end
            if feature["bbox"]              then feature["bbox"] = nil end
            local feature_assets = feature["assets"]
            if feature_assets["browse"]     then feature_assets["browse"] = nil end
            if feature_assets["metadata"]   then feature_assets["metadata"] = nil end
            local feature_properties = feature["properties"]
            for v,k in pairs(feature_assets) do
                feature_properties[v] = feature_assets[v]["href"]
            end
            feature["assets"] = nil
        end
    end
    return geotable
end

---------------------------------------------------------------
-- PUBLIC PACKAGE
---------------------------------------------------------------

--
-- CMR
--
local function cmr ()
end

--
-- STAC
--
local function stac (parms)

    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local dataset = DATASETS[short_name] or {}
    local provider = dataset["provider"] or error("unable to determine provider for query")
    local collections = parms["collections"] or dataset["collections"]
    local polygon = parms["polygon"]
    local t0 = parms["t0"] or '2018-01-01T00:00:00Z'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(time.gps()))

    -- build stac request
    local url = string.format("https://cmr.earthdata.nasa.gov/stac/%s/search", provider)
    local headers = {["Content-Type"] = "application/json"}
    local rqst = {
        ["limit"] = STAC_PAGE_SIZE,
        ["datetime"] = string.format("%s/%s", t0, t1),
        ["collections"] = collections,
    }

    -- add polygon if provided
    if polygon then
        local coordinates = {}
        for k,v in pairs(polygon) do
            table.insert(coordinates, {v["lon"], v["lat"]})
        end
        rqst["intersects"] = {
            ["type"] = "Polygon",
            ["coordinates"] = {coordinates}
        }
    end

    -- post initial request
    local rsps, status = netsvc.post(url, json.encode(rqst), headers)
    local geotable = build_geojson(rsps) 

    -- iterate through additional pages if not all returned
    local num_returned = geotable["context"]["returned"]
    local num_matched = geotable["context"]["matched"]
    if num_matched > DEFAULT_MAX_REQUESTED_RESOURCES then
        sys.log(core.WARNING, string.format("Number of matched resources truncated from %d to %d",num_matched, DEFAULT_MAX_REQUESTED_RESOURCES))
        num_matched = DEFAULT_MAX_REQUESTED_RESOURCES
    end
    local num_pages = math.floor((num_matched  + (num_returned - 1)) / num_returned)
    for page = 2, num_pages do
        rqst["page"] = page
        local rsps, status = netsvc.post(url, json.encode(rqst), headers)
        local paged_geotable = build_geojson(rsps)
        table.move(paged_geotable["features"], 1, #paged_geotable["features"], #geotable["features"] + 1, geotable["features"])
    end
    geotable["context"]["returned"] = num_matched
    geotable["context"]["limit"] = DEFAULT_MAX_REQUESTED_RESOURCES

    -- return response
    return geotable
    
end

--
-- Return Package --
--
return {
    cmr = cmr,
    stac = stac
}
