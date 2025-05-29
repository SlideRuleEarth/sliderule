import geoip2.database
from flask import (g, current_app)

def get_geo():
    if 'geo_country' not in g:
        g.geo_country = geoip2.database.Reader(current_app.config['GEOLITE2_COUNTRY'])
        g.geo_city = geoip2.database.Reader(current_app.config['GEOLITE2_CITY'])
        g.geo_asn = geoip2.database.Reader(current_app.config['GEOLITE2_ASN'])
    return g.geo_country, g.geo_city, g.geo_asn

def close_geo(e=None):
    geo_country = g.pop('geo_country', None)
    geo_city = g.pop('geo_city', None)
    geo_asn = g.pop('geo_asn', None)
    if geo_country is not None:
        geo_country.close()
    if geo_city is not None:
        geo_city.close()
    if geo_asn is not None:
        geo_asn.close()

def init_app(app):
    app.teardown_appcontext(close_geo)