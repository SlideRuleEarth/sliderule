import importlib
import json
import boto3
import random
import string
import argparse
from sliderule import sliderule
from .database import Database, JobState, QueuePriority, JobStatus
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
        # aws clients
        s3 = boto3.client("s3", region_name="us-west-2")

    # Load Remote File from S3
    def __load_remote_file(self, bucket, key):
        obj = self.s3.get_object(Bucket=bucket, Key=key)
        contents = obj["Body"].read().decode("utf-8")
        return json.loads(contents)

    # Get Results for a Job Run
    #   output of job must include the following fields for this to work:
    #   {
    #       "status": <boolean status of run>,
    #       "start": <start time in seconds of run>,
    #       "stop": <stop time in seconds of run>,
    #       "outputs": [<output1>, <output2>, ... <outputN>]
    #   }
    def __get_results(self, run_url):
        results = []
        bucket = run_url.split("s3://")[-1].split("/")[0]
        prefix = "/".join(run_url.split("s3://")[-1].split("/")[1:])
        rsps = self.__load_remote_file(bucket, f"{prefix}/receipt.json") # {"name": ..., "username": ... "args": <path to arg file>, "environment": ...}
        args_list = self.__load_remote_file(bucket, rsps["args"])
        for i in tqdm(range(len(args_list)), total=len(args_list), desc=f"{run_url}", unit="granule"):
            try:
                result = {
                    "file": f"{prefix}/result{i}.json",
                    "environment": rsps["environment"],
                    "arg": args_list[i]
                }
                rsps = self.__load_remote_file(bucket, f"{prefix}/result{i}.json")
                try:
                    result |= {
                        "status": rsps["status"] and JobStatus.SUCCESS or JobStatus.FAILURE,
                        "duration": rsps["status"] and (rsps["stop"] - rsps["start"]) or 0.0,
                        "outputs": rsps["outputs"]
                    }
                except Exception as e:
                    result |= {
                        "status": JobStatus.UNSUPPORTED,
                        "rsps": rsps
                    }
            except Exception as e:
                result = {
                    "status": JobStatus.ERROR,
                    "error": f"{e}"
                }
            results.append(result)
        return results

    # Archive Database
    def archive_database(self):
        self.database.write(self.args.archive)
        self.database.remove()

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

    # Scrape Submissions
    def scrape_submissions(self):
        arg_list = []
        for name,submission in self.database.submissions.items():
            if self.args.name and self.args.name != name: continue
            try:
                if submission["complete"]:
                    print(f"Scraping {name} ...")
                    for result in submission["results"]:
                        if result["status"] in self.args.status:
                            print(f"{result["arg"]}")
                            arg_list.append(result["arg"])
                else:
                    print(f"Skipping {name} because it is still pending")
            except Exception as e:
                print(f"Failed to scrape {name}: {e}")
        if self.args.output:
            with open(self.args.output, "w") as file:
                for arg in arg_list:
                    file.write(f"{arg}\n")

    # Get Status
    def get_status(self):
        queue = self.args.queue
        for name,job in self.database.submissions.items():
            complete = job["complete"]
            print(f"Statusing {name} ...")
            if not complete:
                report = self.session.runner.queue(job_id=job["job_id"], queue=queue)["report"]
                self.database.submissions[name]["status"] = report
                if sum([report[s] for s in [JobState.SUBMITTED, JobState.PENDING, JobState.RUNNABLE, JobState.STARTING, JobState.RUNNING]]) == 0:
                    print(f"Job {name} complete, reading results ...")
                    self.database.submissions[name]["complete"] = True
                    self.database.submissions[name]["results"] = self.__get_results(job["run_url"])
                else:
                    print(f"Job {name} still pending")
        print(",".join([f"{c:>30}" for c in ["NAME"]] + [f"{c:>10}" for c in list(JobState)]))
        for name,job in self.database.submissions.items():
            print(",".join([f"{c:>30}" for c in [name]] + [f"{c:>10}" for c in [job["status"][state] for state in list(JobState)]]))

    # Generate Report
    def generate_report(self):
        stats = {status.value: 0 for status in JobStatus}
        duration = {"avg": 0.0, "total": 0.0}
        processed = 0
        pending = 0
        errors = 0
        for name,submission in self.database.submissions.items():
            try:
                if submission["complete"]:
                    for result in submission["results"]:
                        stats[result["status"]] += 1
                        duration["total"] += result["duration"]
                    processed += 1
                else:
                    pending += 1
            except Exception as e:
                errors += 1
        if processed > 0:
            duration["avg"] = duration["total"] / processed
        print("Processed:", processed)
        print("Pending:", pending)
        print("Errors:", errors)
        print("Status:", json.dumps(stats, indent=2))
        print("Duration:", json.dumps(duration, indent=2))

    # Finish
    def finish(self):
        if not self.args.dryrun:
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
    common.add_argument('--dryrun',     action='store_true',    default=False)

    # command line arguments
    parser = argparse.ArgumentParser(prog="sliderule-runner", description="""SlideRule Runner""")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # archive
    archive = subparsers.add_parser("archive", parents=[common], help="save and clear the database")
    archive.add_argument('archive', metavar="<full path to archive file>")
    archive.set_defaults(func=Tool.archive_database)

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

    # scrape
    scrape = subparsers.add_parser("scrape", parents=[common], help="generate list of arguments from jobs with provided job status")
    scrape.add_argument('--status',     type=JobStatus, nargs='+',  default=[JobStatus.FAILURE, JobStatus.UNSUPPORTED, JobStatus.ERROR])
    scrape.add_argument('--name',       type=str,                   default=None) # name of submission
    scrape.add_argument('--output',     type=str,                   default=None)
    scrape.set_defaults(func=Tool.scrape_submissions)

    # status
    status = subparsers.add_parser("status", parents=[common], help="display status of submitted jobs")
    status.set_defaults(func=Tool.get_status)

    # report
    report = subparsers.add_parser("report", parents=[common], help="generate report of submitted jobs")
    report.set_defaults(func=Tool.generate_report)

    # parse command line
    args = parser.parse_args()

    # create tool
    tool = Tool(args)

    # route command
    try:
        args.func(tool)
        tool.finish() # only execute if tool function completed successfully
    except Exception as e:
        if args.verbose: raise
        print(f"Unhandled error: {e}")

# running via direct invocation
if __name__ == "__main__": main()
