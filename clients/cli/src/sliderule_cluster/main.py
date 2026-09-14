import json
import uuid
import tempfile
import argparse
from pathlib import Path
from sliderule import sliderule

#########################################
# Tool
#########################################

class Tool:

    # constructor
    def __init__(self, args):
        self.args = args
        self.session = sliderule.create_session(domain=args.domain, cluster=args.cluster, user_service=args.user_service, verbose=args.verbose)

    # display_result
    def __display_result(self, result, reformat=False):
        if reformat:
            result = json.dumps(result, indent=2)
        print(f'{self.args.cluster}.{self.args.domain} [{self.session.service}]: {result}')

    # get_parms
    def __get_parms(self, parms=None):
        if self.args.parms:
            with open(self.args.parms, "r") as file:
                parms = json.load(file) # override
        return parms

    # get_poly
    def __get_poly(self):
        if self.args.poly:
            return sliderule.toregion(self.args.poly)["poly"]
        elif self.args.geojson:
            return sliderule.toregion(self.args.geojson)["poly"]
        elif self.args.bbox:
            return sliderule.toregion(self.args.bbox)["poly"]
        else:
            return None

    # whoami
    def whoami(self):
        rsps = self.session.source("whoami", parm={}, path="/discovery", retries=0, rethrow=True)
        result = json.dumps(rsps)
        self.__display_result(result)
        return result

    # status
    def status(self):
        if self.session.service != self.session.PUBLIC_CLUSTER:
            parm = {"service":self.session.service}
        else:
            parm = {}
        rsps = self.session.source("status", parm=parm, path="/discovery", retries=0, rethrow=True)
        result = json.dumps(rsps)
        self.__display_result(result)
        return result

    # version
    def version(self):
        result = sliderule.get_version(session=self.session)
        self.__display_result(result, reformat=True)
        return result

    # defaults
    def defaults(self):
        result = sliderule.source("defaults", session=self.session)
        self.__display_result(result, reformat=True)
        return result

    # earthdata
    def earthdata(self):
        parms = self.__get_parms({ k: v for k, v in {
            "asset": self.args.asset,
            "short_name": self.args.short_name,
            "poly": self.__get_poly(),
            "t0": self.args.t0,
            "t1": self.args.t1,
            "with_meta": self.args.with_meta,
            "name_filter": self.args.name_filter,
            "max_resources": self.args.max_resources
        }.items() if v is not None })
        result = sliderule.source("earthdata", parms, rethrow=True, session=self.session)
        self.__display_result(result, reformat=True)
        return result

    # run
    def run(self):
        # get request parameters
        if self.args.parms.startswith('"') or self.args.parms.startswith('{'):
            parms = self.args.parms
        else:
            parms = self.__get_parms()
        # build output parameters (if not supplied in request)
        if "output" not in parms:
            output_format = "geoparquet"
            output_path = str(Path(tempfile.gettempdir()) / f"{uuid.uuid4()}.{output_format}")
            parms |= {
                "output": {
                    "path": output_path,
                    "format": output_format
                }
            }
        elif "path" in parms["output"]:
            output_path = parms["output"]["path"]
        else:
            output_path = None # will need to look for it later
        # make request
        rsps = sliderule.source(self.args.api, {"parms": parms}, stream=True, session=self.session)
        # find remote output path (if not already set)
        if not output_path:
            for rsp in rsps:
                if 'arrowrec.remote' == rsp['__rectype']:
                    output_path = rsp['url']
                    break
        # set and return results
        result = output_path
        self.__display_result(result, reformat=True)
        return result

#########################################
# Main
#########################################

def main():

    # options shared by every subcommand; parents= lets them appear after the command name
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('--domain',         type=str,               default="slideruleearth.io")
    common.add_argument('--cluster',        type=str,               default="sliderule")
    common.add_argument('--user_service',   action='store_true',    default=False, help="uses dedicated user capacity")
    common.add_argument('--verbose',        action='store_true',    default=False, help="turns on verbose log messages")
    common.add_argument('--result',         type=str,               default=None, help="file name to store result of command")

    # command line arguments
    parser = argparse.ArgumentParser(prog="sliderule-cluster", description="""SlideRule Cluster""")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # whoami
    whoami = subparsers.add_parser("whoami", parents=[common], help="self identification of the cluster; returns cluster name")
    whoami.set_defaults(func=Tool.whoami)

    # status
    status = subparsers.add_parser("status", parents=[common], help="registration status of cluster")
    status.set_defaults(func=Tool.status)

    # version
    version = subparsers.add_parser("version", parents=[common], help="cluster version information")
    version.set_defaults(func=Tool.version)

    # defaults
    defaults = subparsers.add_parser("defaults", parents=[common], help="cluster default parameter values")
    defaults.set_defaults(func=Tool.defaults)

    # earthdata
    earthdata = subparsers.add_parser("earthdata", parents=[common], help="earthdata search")
    earthdata.add_argument('--asset',           type=str,               default="icesat2", help="SlideRule asset name")
    earthdata.add_argument('--poly',            type=float, nargs='+',  default=None, help="closed counter-clockwise list of latitude longitude pairs defining the area of interest; e.g. lon1 lat1 lon2 lat2 lon3 lat3 ... lon1 lat1")
    earthdata.add_argument('--geojson',         type=str,               default=None, help="geojson filename defining the area of interest")
    earthdata.add_argument('--bbox',            type=float, nargs='+',  default=None, help="bounding box defining the area of interest; e.g. lon_ll lat_ll lon_ur lat_ur")
    earthdata.add_argument('--t0',              type=str,               default=None, help="start time as an ISO datetime string YYYY-MM-DDTHH:MM:SSZ") # datetime.fromisoformat(args.t0)
    earthdata.add_argument('--t1',              type=str,               default=None, help="stop time as an ISO datetime string YYYY-MM-DDTHH:MM:SSZ") # datetime.fromisoformat(args.t1)
    earthdata.add_argument('--max_resources',   type=int,               default=None, help="maximum number of resources allowed to be returned by query; queries exceeding this number return an error")
    earthdata.add_argument('--short_name',      type=str,               default=None, help="CMR dataset name")
    earthdata.add_argument('--with_meta',       action='store_true',    default=False, help="return metadata along with query results")
    earthdata.add_argument('--name_filter',     type=str,               default=None, help="regular expression to evaluate against resource names returned by query")
    earthdata.set_defaults(func=Tool.earthdata)

    # run
    run = subparsers.add_parser("run", parents=[common], help="execute dataframe based api request")
    run.add_argument('api',     type=str,   metavar="<api>", help="endpoint being called")
    run.add_argument('--parms', type=str,   required=True, help="string or filename containing json parameters to use in request")
    run.set_defaults(func=Tool.run)

    # parse command line
    args = parser.parse_args()

    # create tool
    tool = Tool(args)

    # route command
    try:
        result = args.func(tool)
        if args.result:
            with open(args.result, "w") as file:
                file.write(result)
    except Exception as e:
        if args.verbose: raise
        print(f"Unhandled error: {e}")

# running via direct invocation
if __name__ == "__main__": main()
