import os
DATABASE = os.getenv("DUCKDB_LOCAL_FILE") or '/data/manager.db'
REMOTE_DATABASE = os.getenv("DUCKDB_REMOTE_FILE") or None
SECRET_SALT = os.getenv("MANAGER_SECRET_SALT") or ""
API_KEY = os.getenv("MANAGER_API_KEY") or ""
GEOLITE2_ASN = '/data/GeoLite2-ASN.mmdb'
GEOLITE2_CITY = '/data/GeoLite2-City.mmdb'
GEOLITE2_COUNTRY = '/data/GeoLite2-Country.mmdb'
ATL13_MAPPINGS = '/data/ATL13/atl13_mappings.json'
ATL13_MASK = '/data/ATL13/inland_water_body.parquet'
ENDPOINT_TLM_EXCLUSION = ['prometheus', 'health']
ORCHESTRATOR = os.getenv("ORCHESTRATOR") or "http://127.0.0.1:8050"
RATELIMIT_WEEKLY_COUNT = os.getenv("RATELIMIT_WEEKLY_COUNT") and int(os.getenv("RATELIMIT_WEEKLY_COUNT")) or 50000 # maximum number of requests that can be made in one week
RATELIMIT_BACKOFF_COUNT = os.getenv("RATELIMIT_BACKOFF_COUNT") and int(os.getenv("RATELIMIT_BACKOFF_COUNT")) or 10 # after the max count is reached, this is the number of new requests that can be made before the next backoff
RATELIMIT_BACKOFF_PERIOD = os.getenv("RATELIMIT_BACKOFF_PERIOD") and int(os.getenv("RATELIMIT_BACKOFF_PERIOD")) or 10 * 60 # seconds, this user will not be able to make requests for this long