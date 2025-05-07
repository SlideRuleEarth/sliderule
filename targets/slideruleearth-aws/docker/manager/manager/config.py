import os
DATABASE = os.getenv("DUCKDB_FILE") or '/data/manager.db'
SECRET_SALT = os.getenv("MANAGER_SECRET_SALT") or ""
GEOLITE2_ASN = '/data/GeoLite2-ASN.mmdb'
GEOLITE2_CITY = '/data/GeoLite2-City.mmdb'
GEOLITE2_COUNTRY = '/data/GeoLite2-Country.mmdb'
ENDPOINT_TLM_EXCLUSION = ['prometheus', 'health']