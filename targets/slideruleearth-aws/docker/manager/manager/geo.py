import geoip2.database
from flask import current_app

####################
# Initialization
####################

geo_country = None
geo_city = None
geo_asn = None

####################
# Functions
####################

def get_geo():
    global geo_country, geo_city, geo_asn
    if type(geo_country) == type(None):
        geo_country = geoip2.database.Reader(current_app.config['GEOLITE2_COUNTRY'])
        geo_city = geoip2.database.Reader(current_app.config['GEOLITE2_CITY'])
        geo_asn = geoip2.database.Reader(current_app.config['GEOLITE2_ASN'])
    return geo_country, geo_city, geo_asn

def close_geo(e=None):
    global geo_country, geo_city, geo_asn
    if type(geo_country) != type(None):
        geo_country.close()
    if type(geo_city) != type(None):
        geo_city.close()
    if type(geo_asn) != type(None):
        geo_asn.close()

def init_app(app):
    app.teardown_appcontext(close_geo)

