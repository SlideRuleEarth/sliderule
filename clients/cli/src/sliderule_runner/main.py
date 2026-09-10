import importlib
import sys
import random
import string
import argparse
from sliderule import sliderule
from .database import Database, JobState, QueuePriority
from pathlib import Path

try:
    tqdm = importlib.import_module("tqdm").tqdm
except Exception:
    print(f"tqdm unavailable, progress of operations will not be reported")
    def tqdm(iterable, **kwargs):
        return iterable

#########################################
# Tool
#########################################

class Tool:

    # Constructor
    def __init__(self, args):
        self.args = args
        # open database
        self.database = Database(args.database)
        # create sliderule session
        self.session = sliderule.create_session(verbose=args.verbose)
        self.session.authenticate() # gives privileges to access SlideRule Runner

    # Submit Job
    def submit_job(self):
        # pull out arguments
        name = self.args.name
        script_file = self.args.script
        arguments_file = self.args.arguments
        batch_size = self.args.batch_size
        vcpus = self.args.vcpus
        memory = self.args.memory
        image = self.args.image
        queue = self.args.queue
        # read script
        with open(script_file, "r") as file:
            script = file.read()
        # read arguments
        with open(arguments_file, "r") as file:
            arguments = [line.strip() for line in file.readlines()]
        # process job in batches
        for i in range(0, len(arguments), batch_size):
            # build and check name
            job_name = f"{name}_{i}"
            if job_name in self.database.submissions:
                unique = ''.join(random.choices(string.ascii_lowercase, k=3))
                job_name = f"{name}_{unique}_{i}"
            # submit & save job
            args_list = arguments[i:i+batch_size]
            rsps = self.session.runner.submit(name=job_name, script=script, args=args_list, optional_args={"vcpus":vcpus, "memory":memory, "image":image, "queue":queue})
            self.database.submissions[job_name] = rsps | {"complete": False}
            print(f"Submitted job {job_name} using script {script_file} with {len(args_list)} entries: {rsps}")

    # Get Status
    def get_status(self):
        queue = self.args.queue
        for name,job in self.database.submissions.items():
            complete = job["complete"]
            print(f"Statusing {name} - ", end='')
            if not complete:
                status = self.session.runner.queue(job_id=job["job_id"], queue=queue)["report"]
                self.database.submissions[name]["status"] = status
                if sum([status[s] for s in [JobState.SUBMITTED, JobState.PENDING, JobState.RUNNABLE, JobState.STARTING, JobState.RUNNING]]) == 0:
                    self.database.submissions[name]["complete"] = True
                    complete = True
            print(f"{complete and 'complete' or 'incomplete'}")
        print(",".join([f"{c:>30}" for c in ["NAME"]] + [f"{c:>10}" for c in list(JobState)]))
        for name,job in self.database.submissions.items():
            print(",".join([f"{c:>30}" for c in [name]] + [f"{c:>10}" for c in [job["status"][state] for state in list(JobState)]]))

    # Finish
    def finish(self):
        # save database
        self.database.write()

#########################################
# Main
#########################################

def main():

    # options shared by every subcommand; parents= lets them appear after the command name
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('--database',   type=Path,              default=Path.home() / ".cache" / "sliderule" / "runner_database.json")
    common.add_argument('--queue',      type=QueuePriority,     default=QueuePriority.DEFAULT, choices=list(QueuePriority))
    common.add_argument('--verbose',    action='store_true',    default=False)

    # command line arguments
    parser = argparse.ArgumentParser(prog="sliderule-runner", description="""SlideRule Runner""")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # submit
    submit = subparsers.add_parser("submit", parents=[common], help="submit a job")
    submit.add_argument('name',         metavar="<name>")
    submit.add_argument('script',       metavar="<script.lua>",     type=Path)
    submit.add_argument('arguments',    metavar="<arguments.txt>",  type=Path)
    submit.add_argument('--vcpus',      type=int,                   default=4)
    submit.add_argument('--memory',     type=int,                   default=16000)
    submit.add_argument('--batch_size', type=int,                   default=10000)
    submit.add_argument('--image',      type=str,                   default="sliderule:latest")
    submit.set_defaults(func=Tool.submit_job)

    # status
    status = subparsers.add_parser("status", parents=[common], help="report status of submitted jobs")
    status.set_defaults(func=Tool.get_status)

    # parse command line
    args = parser.parse_args()

    # create tool
    tool = Tool(args)

    # route command
    try:
        args.func(tool)
    except Exception as e:
        if args.verbose: raise
        print(f"Unhandled error: {e}")
    finally:
        tool.finish()


# running via direct invocation
if __name__ == "__main__": main()
