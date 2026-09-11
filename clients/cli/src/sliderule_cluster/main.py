import json
import argparse
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
    common.add_argument('--output',         type=str,               default=None, help="file name to store output of command")

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

    # parse command line
    args = parser.parse_args()

    # create tool
    tool = Tool(args)

    # route command
    try:
        result = args.func(tool)
        if args.output:
            with open(args.output, "w") as file:
                file.write(result)
    except Exception as e:
        if args.verbose: raise
        print(f"Unhandled error: {e}")

# running via direct invocation
if __name__ == "__main__": main()
