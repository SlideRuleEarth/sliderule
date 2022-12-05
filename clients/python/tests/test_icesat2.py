"""Tests for sliderule-python icesat2 api."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule
from sliderule import icesat2
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

@pytest.fixture(scope='module')
def grandmesa():
    return [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
               {"lon": -107.7677425431139, "lat": 38.90611184543033},
               {"lon": -107.7818591266989, "lat": 39.26613714985466},
               {"lon": -108.3605610678553, "lat": 39.25086131372244},
               {"lon": -108.3435200747503, "lat": 38.89102961045247} ]


class TestLocal:
    def test_init_empty_raises(self):
        with pytest.raises(TypeError, match=('url')):
            icesat2.init()

    def test_toregion_empty_raises(self):
        with pytest.raises(TypeError, match=('source')):
            region = icesat2.toregion()

    def test_toregion(self):
        region = icesat2.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        assert len(region["poly"]) == 5 # 5 coordinate pairs
        assert {'lon', 'lat'} <= region["poly"][0].keys()

@pytest.mark.network
class TestRemote:
    def test_init_badurl(self):
        with pytest.raises( (sliderule.FatalError) ):
            icesat2.init('incorrect.org:8877')
            sliderule.source("version")

    def test_get_version(self, domain, organization):
        icesat2.init(domain, organization=organization)
        version = icesat2.get_version()
        assert isinstance(version, dict)
        assert {'icesat2', 'server', 'client'} <= version.keys()

    def test_cmr(self, grandmesa, domain, organization):
        icesat2.init(domain, organization=organization)
        granules = icesat2.cmr(polygon=grandmesa,
            time_start='2018-10-01',
            time_end='2018-12-01')
        assert isinstance(granules, list)
        assert 'ATL03_20181017222812_02950102_005_01.h5' in granules
