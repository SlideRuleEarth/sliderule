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

    # initialize ATL13
    from . import atl13
    atl13.init_app(app)
    app.register_blueprint(atl13.atl13)

    # initialize ATL24
    from . import atl24
    atl24.init_app(app)
    app.register_blueprint(atl24.atl24)

    # initialize 3DEP
    from . import usgs3dep
    usgs3dep.init_app(app)
    app.register_blueprint(usgs3dep.usgs3dep)

    return app