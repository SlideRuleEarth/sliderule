"""Tests for sliderule icesat2 atl06-sr algorithm."""

import pytest
import os
import requests
import tempfile
import pandas
import json
from sliderule import sliderule

def make_rqst(endpoint, domain, organization, data):
    if organization == None:
        url = f'http://{domain}/arrow/{endpoint}'
    else:
        url = f'https://{organization}.{domain}/arrow/{endpoint}'
        print(f'URL = {url}')
    data = {"parms": data}
    session = requests.Session()
    session.trust_env = False
    tmpname = os.path.join(tempfile.gettempdir(), next(tempfile._get_candidate_names()))
    try:
        with requests.post(url, data=json.dumps(data), timeout=60, stream=True) as response:
            response.raise_for_status()
            with open(tmpname, "wb") as f:
                for chunk in response.iter_content(chunk_size=0x2000000):
                    if chunk:
                        print(f'Writing chunk of size {len(chunk)}')
                        f.write(chunk)
        print(f'Opening parquet file: {tmpname}')
        df = pandas.read_parquet(tmpname)
        print(f'Finished reading parquet file: {tmpname}')
    except Exception as e:
        print(f'Failed to make request to {url}: {e}')
        df = None
    if os.path.exists(tmpname):
        os.remove(tmpname)
    return df

class TestArrow:
    def test_atl24x(self, domain, organization):
        df = make_rqst("atl24x", domain, organization, {
            "output": {"format": "parquet"},
            "resources": ["ATL24_20181014001920_02350103_006_02_001_01.h5"]
        })
        if organization == None or organization == sliderule.session.Session.PUBLIC_URL: # otherwise need to build authentication headers
            assert isinstance(df, pandas.DataFrame) or isinstance(df, pandas.core.frame.DataFrame)
            assert len(df) == 63
            assert len(df.keys()) == 14 # this is not a GDF, so there is one more column since lat and lon are not combined into geometry
            assert df["gt"].sum() == 2160
        else:
            assert df == None

    def test_atl03x(self, domain, organization):
        df = make_rqst("atl03x", domain, organization, {
            "output": {"format": "parquet"},
            "resources": ["ATL03_20181019065445_03150111_006_02.h5"],
            "beams": "gt1l",
            "cnf": 4
        })
        if organization == None or organization == sliderule.session.Session.PUBLIC_URL: # otherwise need to build authentication headers
            assert isinstance(df, pandas.DataFrame) or isinstance(df, pandas.core.frame.DataFrame)
            assert len(df) == 5814857
            assert len(df.keys()) == 17
            assert df["gt"].sum() == 58148570
        else:
            assert df == None
