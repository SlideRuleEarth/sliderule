import os
DATABASE = '/data/manager.db'
SECRET_SALT = os.getenv("MANAGER_SECRET_SALT") or ""
GEOLITE2_ASN = '/data/GeoLite2-ASN_20250424/GeoLite2-ASN.mmdb'
GEOLITE2_CITY = '/data/GeoLite2-City_20250422/GeoLite2-City.mmdb'
GEOLITE2_COUNTRY = '/data/GeoLite2-Country_20250422/GeoLite2-Country.mmdb'