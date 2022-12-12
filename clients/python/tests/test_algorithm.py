"""Tests for sliderule icesat2 atl06-sr algorithm."""

import pytest
from pyproj import Transformer
from shapely.geometry import Polygon, Point
import pandas as pd
from sliderule import icesat2

@pytest.mark.network
class TestAlgorithm:
    def test_atl06(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource = "ATL03_20181019065445_03150111_004_01.h5"
        parms = { "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl06(parms, resource, asset)
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["h_mean"]) == 622423

    def test_atl06p(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource = "ATL03_20181019065445_03150111_004_01.h5"
        parms = { "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl06p(parms, asset, resources=[resource])
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["h_mean"]) == 622423

    def test_atl03s(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource = "ATL03_20181019065445_03150111_004_01.h5"
        region = [ { "lat": -80.75, "lon": -70.00 },
                   { "lat": -81.00, "lon": -70.00 },
                   { "lat": -81.00, "lon": -65.00 },
                   { "lat": -80.75, "lon": -65.00 },
                   { "lat": -80.75, "lon": -70.00 } ]
        parms = { "poly": region,
                  "track": 1,
                  "cnf": 0,
                  "pass_invalid": True,
                  "yapc": { "score": 0 },
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"],
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 20.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl03s(parms, resource, asset)
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["height"]) == 488673

    def test_atl03sp(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource = "ATL03_20181019065445_03150111_004_01.h5"
        region = [ { "lat": -80.75, "lon": -70.00 },
                   { "lat": -81.00, "lon": -70.00 },
                   { "lat": -81.00, "lon": -65.00 },
                   { "lat": -80.75, "lon": -65.00 },
                   { "lat": -80.75, "lon": -70.00 } ]
        parms = { "poly": region,
                  "track": 1,
                  "cnf": 0,
                  "pass_invalid": True,
                  "yapc": { "score": 0 },
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"],
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 20.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl03sp(parms, asset, resources=[resource])
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["height"]) == 488673

    def test_atl08(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource = "ATL03_20181213075606_11560106_004_01.h5"
        track = 1
        region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                   {"lon": -107.7677425431139, "lat": 38.90611184543033},
                   {"lon": -107.7818591266989, "lat": 39.26613714985466},
                   {"lon": -108.3605610678553, "lat": 39.25086131372244},
                   {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        parms = { "poly": region,
                  "track": track,
                  "cnf": 0,
                  "pass_invalid": True,
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"],
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 20.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl03s(parms, resource, asset)
        assert min(gdf["rgt"]) == 1156
        assert min(gdf["cycle"]) == 1
        assert len(gdf["height"]) == 243237
        assert len(gdf[gdf["atl08_class"] == 0]) == 30493
        assert len(gdf[gdf["atl08_class"] == 1]) == 123485
        assert len(gdf[gdf["atl08_class"] == 2]) == 54251
        assert len(gdf[gdf["atl08_class"] == 3]) == 18958
        assert len(gdf[gdf["atl08_class"] == 4]) == 16050

    def test_gs(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource_prefix = "20210114170723_03311012_004_01.h5"
        region = [ {"lon": 126.54560629670780, "lat": -70.28232209449946},
                   {"lon": 114.29798416287946, "lat": -70.08880029415151},
                   {"lon": 112.05139144652648, "lat": -74.18128224472123},
                   {"lon": 126.62732471857403, "lat": -74.37827832634999},
                   {"lon": 126.54560629670780, "lat": -70.28232209449946} ]

        # Make ATL06-SR Processing Request
        parms = { "poly": region,
                  "cnf": 4,
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 2.0,
                  "res": 1.0,
                  "dist_in_seg": True,
                  "maxi": 10 }
        sliderule = icesat2.atl06(parms, "ATL03_"+resource_prefix, asset)

        # Project Region to Polygon #
        transformer = Transformer.from_crs(4326, 3857) # GPS to Web Mercator
        pregion = []
        for point in region:
            ppoint = transformer.transform(point["lat"], point["lon"])
            pregion.append(ppoint)
        polygon = Polygon(pregion)

        # Read lat,lon from resource
        tracks = ["1l", "1r", "2l", "2r", "3l", "3r"]
        geodatasets = [{"dataset": "/orbit_info/sc_orient"}]
        for track in tracks:
            prefix = "/gt"+track+"/land_ice_segments/"
            geodatasets.append({"dataset": prefix+"latitude", "startrow": 0, "numrows": -1})
            geodatasets.append({"dataset": prefix+"longitude", "startrow": 0, "numrows": -1})
        geocoords = icesat2.h5p(geodatasets, "ATL06_"+resource_prefix, asset)

        # Build list of the subsetted h_li datasets to read
        hidatasets = []
        for track in tracks:
            prefix = "/gt"+track+"/land_ice_segments/"
            startrow = -1
            numrows = -1
            index = 0
            for index in range(len(geocoords[prefix+"latitude"])):
                lat = geocoords[prefix+"latitude"][index]
                lon = geocoords[prefix+"longitude"][index]
                c = transformer.transform(lat, lon)
                point = Point(c[0], c[1])
                intersect = point.within(polygon)
                if startrow == -1 and intersect:
                    startrow = index
                elif startrow != -1 and not intersect:
                    numrows = index - startrow
                    break
            hidatasets.append({"dataset": prefix+"h_li", "startrow": startrow, "numrows": numrows, "prefix": prefix})
            hidatasets.append({"dataset": prefix+"segment_id", "startrow": startrow, "numrows": numrows, "prefix": prefix})

        # Read h_li from resource
        hivalues = icesat2.h5p(hidatasets, "ATL06_"+resource_prefix, asset)

        # Build Results #
        atl06 = {"h_mean": [], "lat": [], "lon": [], "segment_id": [], "spot": []}
        prefix2spot = { "/gt1l/land_ice_segments/": {0: 1, 1: 6},
                        "/gt1r/land_ice_segments/": {0: 2, 1: 5},
                        "/gt2l/land_ice_segments/": {0: 3, 1: 4},
                        "/gt2r/land_ice_segments/": {0: 4, 1: 3},
                        "/gt3l/land_ice_segments/": {0: 5, 1: 2},
                        "/gt3r/land_ice_segments/": {0: 6, 1: 1} }
        for entry in hidatasets:
            if "h_li" in entry["dataset"]:
                atl06["h_mean"] += hivalues[entry["prefix"]+"h_li"].tolist()
                atl06["lat"] += geocoords[entry["prefix"]+"latitude"][entry["startrow"]:entry["startrow"]+entry["numrows"]].tolist()
                atl06["lon"] += geocoords[entry["prefix"]+"longitude"][entry["startrow"]:entry["startrow"]+entry["numrows"]].tolist()
                atl06["segment_id"] += hivalues[entry["prefix"]+"segment_id"].tolist()
                atl06["spot"] += [prefix2spot[entry["prefix"]][geocoords["/orbit_info/sc_orient"][0]] for i in range(entry["numrows"])]

        # Build DataFrame of ATL06 NSIDC Data #
        nsidc = pd.DataFrame(atl06)

        # Add Lat and Lon Columns to SlideRule DataFrame
        sliderule["lon"] = sliderule.geometry.x
        sliderule["lat"] = sliderule.geometry.y

        # Initialize Error Variables #
        diff_set = ["h_mean", "lat", "lon"]
        errors = {}
        total_error = {}
        segments = {}
        orphans = {"segment_id": [], "h_mean": [], "lat": [], "lon": []}

        # Create Segment Sets #
        for index, row in nsidc.iterrows():
            segment_id = row["segment_id"]
            # Create Difference Row for Segment ID #
            if segment_id not in segments:
                segments[segment_id] = {}
                for spot in [1, 2, 3, 4, 5, 6]:
                    segments[segment_id][spot] = {}
                    for process in ["sliderule", "nsidc", "difference"]:
                        segments[segment_id][spot][process] = {}
                        for element in diff_set:
                            segments[segment_id][spot][process][element] = 0.0
            for element in diff_set:
                segments[segment_id][row["spot"]]["nsidc"][element] = row[element]
        for index, row in sliderule.iterrows():
            segment_id = row["segment_id"]
            if segment_id not in segments:
                orphans["segment_id"].append(segment_id)
            else:
                for element in diff_set:
                    segments[segment_id][row["spot"]]["sliderule"][element] = row[element]
                    segments[segment_id][row["spot"]]["difference"][element] = segments[segment_id][row["spot"]]["sliderule"][element] - segments[segment_id][row["spot"]]["nsidc"][element]

        # Flatten Segment Sets to just Differences #
        error_threshold = 1.0
        for element in diff_set:
            errors[element] = []
            total_error[element] = 0.0
            for segment_id in segments:
                for spot in [1, 2, 3, 4, 5, 6]:
                    error = segments[segment_id][spot]["difference"][element]
                    if(abs(error) > error_threshold):
                        orphans[element].append(error)
                    else:
                        errors[element].append(error)
                        total_error[element] += abs(error)

        # Asserts
        assert min(sliderule["rgt"]) == 331
        assert min(sliderule["cycle"]) == 10
        assert len(sliderule) == 55367
        assert len(nsidc) == 55691
        assert len(orphans["segment_id"]) == 1671
        assert len(orphans["h_mean"]) == 204
        assert len(orphans["lat"]) == 204
        assert len(orphans["lon"]) == 204
        assert abs(total_error["h_mean"] - 1723.8) < 0.1
        assert abs(total_error["lat"] - 0.045071) < 0.001
        assert abs(total_error["lon"] - 0.022374) < 0.001
