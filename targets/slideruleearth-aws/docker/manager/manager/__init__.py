import os
from flask import Flask

# Set Version
version_found = False
possible_version_paths = [
    '../../../../../version.txt', # repository
    '/version.txt' # docker
]
for path in possible_version_paths:
    if os.path.exists(path):
        with open(path) as file:
            __version__ = file.read()[1:].strip()
            version_found = True
            break
if not version_found:
    __version__ = "0.0.0"

#
# Application Factory
#
def create_app(test_config=None):

    # create and configure the app
    app = Flask(__name__, instance_relative_config=False)
    if test_config is None:
        # load the instance config, if it exists, when not testing
        app.config.from_pyfile('config.py', silent=True)
    else:
        # load the test config if passed in
        app.config.from_mapping(test_config)

    # echo test route
    @app.route('/echo/<var>')
    def echo(var):
        return f'{var}'

    # initialize database
    from . import db
    db.init_app(app)
    app.register_blueprint(db.db)

    # initialize geolite2
    from . import geo
    geo.init_app(app)

    # initialize asset metadata service
    # bring in ams subsystem
    from . import ams
    ams.init_app(app)
    app.register_blueprint(ams.ams)

    # bring in telemetry subsystem
    from . import telemetry
    app.register_blueprint(telemetry.telemetry)

    # bring in alerts subsystem
    from . import alerts
    app.register_blueprint(alerts.alerts)

    # bring in status subsystem
    from . import status
    app.register_blueprint(status.status)

    return app