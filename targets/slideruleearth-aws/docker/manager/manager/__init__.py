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

    # initialize geolite2
    from . import geo
    geo.init_app(app)

    # bring in metrics subsystem
    from . import metrics
    app.register_blueprint(metrics.metrics)

    # bring in status subsystem
    from . import status
    app.register_blueprint(status.status)

    return app