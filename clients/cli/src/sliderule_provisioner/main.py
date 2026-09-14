import json
import argparse
from datetime import datetime
from zoneinfo import ZoneInfo
from sliderule import sliderule

#########################################
# Tool
#########################################

class Tool:

    # Constructor
    def __init__(self, args):
        self.args = args
        self.session = sliderule.create_session(domain=args.domain, cluster=args.cluster, user_service=args.user_service, verbose=args.verbose)

    def __display_concise(self, response):
        data = {
            "error": response.get("error"),
            "error_description": response.get("error_description"),
            "stack_name": response.get("response", {}).get("StackName"),
            "stack_status": response.get("response", {}).get("StackStatus"),
            "creation_time": response.get("response", {}).get("CreationTime"),
            "auto_shutdown": response.get("auto_shutdown"),
            "current_nodes": response.get("current_nodes"),
            "version": response.get("version"),
            "is_public": response.get("is_public"),
            "node_capacity": response.get("node_capacity"),
            "users": response.get("users")
        }
        for date_field in ["creation_time", "auto_shutdown"]:
            if data[date_field]:
                data[date_field] = datetime.fromisoformat(data[date_field]).astimezone(ZoneInfo(self.args.timezone)).strftime("%Y-%m-%d %H:%M:%S %Z")
        if data["users"]:
            for user in data["users"]:
                data["users"][user]["auto_shutdown"] = datetime.fromisoformat(data["users"][user]["auto_shutdown"]).astimezone(ZoneInfo(self.args.timezone)).strftime("%Y-%m-%d %H:%M:%S %Z")
        return {k:data[k] for k in data if data[k] is not None}

    # Deploy
    def deploy(self):
        return self.session.provisioner.deploy(is_public=(self.args.is_public == "true"), node_capacity=self.args.node_capacity, ttl=self.args.ttl, version=self.args.version)

    # Extend
    def extend(self):
        return self.session.provisioner.extend(ttl=self.args.ttl)

    # Destroy
    def destroy(self):
        return self.session.provisioner.destroy()

    # Status
    def status(self):
        return self.__display_concise(self.session.provisioner.status())

    # Events
    def events(self):
        return self.session.provisioner.events()

    # Report
    def report(self):
        return {k[0]:self.__display_concise(k[1]) for k in self.session.provisioner.report(kind=self.args.kind).items()}

    # Test
    def test(self):
        return self.session.gateway_request("test", subdomain="provisioner", data={"branch":self.args.branch})

    # Info
    def info(self):
        return self.session.provisioner.info()

    # Authenticate
    def authenticate(self):
        return self.session.authenticate(force_login=True)


#########################################
# Main
#########################################

def main():

    # command line arguments
    parser = argparse.ArgumentParser(prog="sliderule-runner", description="""SlideRule Runner""")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # options shared by every subcommand; parents= lets them appear after the command name
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('--domain',         type=str,               default="slideruleearth.io")
    common.add_argument('--cluster',        type=str,               default="developers")
    common.add_argument('--user_service',   action='store_true',    default=False)
    common.add_argument('--concise',        action='store_true',    default=False)
    common.add_argument('--verbose',        action='store_true',    default=False)
    common.add_argument('--timezone',       type=str,               default="America/New_York")

    # deploy
    deploy = subparsers.add_parser("deploy", parents=[common], help="deploy a cluster (stand-alone or as user capacity)")
    deploy.add_argument('--is_public',      type=str,   default="false")
    deploy.add_argument('--node_capacity',  type=int,   default=None)
    deploy.add_argument('--ttl',            type=int,   default=60) # 1 hour
    deploy.add_argument('--version',        type=str,   default="unstable")
    deploy.set_defaults(func=Tool.deploy)

    # extend
    extend = subparsers.add_parser("extend", parents=[common], help="extend a cluster")
    extend.add_argument('--ttl',    type=int,   default=60) # 1 hour
    extend.set_defaults(func=Tool.extend)

    # destroy
    destroy = subparsers.add_parser("destroy", parents=[common], help="destroy a cluster")
    destroy.set_defaults(func=Tool.destroy)

    # status
    status = subparsers.add_parser("status", parents=[common], help="status a cluster")
    status.set_defaults(func=Tool.status)

    # events
    events = subparsers.add_parser("events", parents=[common], help="list the stack events associated with a cluster")
    events.set_defaults(func=Tool.events)

    # report
    report = subparsers.add_parser("report", parents=[common], help="report metadata of active clusters")
    report.add_argument('--kind',   type=str,   default="clusters")
    report.set_defaults(func=Tool.report)

    # test
    test = subparsers.add_parser("test", parents=[common], help="execute the test runner")
    test.add_argument('--branch',   type=str,   default="main")
    test.set_defaults(func=Tool.test)

    # info
    info = subparsers.add_parser("info", parents=[common], help="display information about provisioner")
    info.set_defaults(func=Tool.info)

    # authenticate
    authenticate = subparsers.add_parser("authenticate", parents=[common], help="force re-authentication of user account")
    authenticate.set_defaults(func=Tool.authenticate)

    # parse command line
    args = parser.parse_args()

    # create tool
    tool = Tool(args)

    # route command
    try:
        result = args.func(tool)
        print(f'{json.dumps(result, indent=2)}')
    except Exception as e:
        if args.verbose: raise
        print(f"Unhandled error: {e}")

# running via direct invocation
if __name__ == "__main__": main()
