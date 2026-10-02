---
title: sliderule-runner
description: User guide and reference for the sliderule-runner command line tool
---

`sliderule-runner` is a command line tool for running a SlideRule script over a large list of inputs (for example, a list of granules) as a set of batch jobs. It can be used to submit jobs, monitor their progress, summarize their results, collect the inputs that failed so they can be resubmitted, retrieve job logs, and cancel jobs. The tool keeps a local database of everything you have submitted so that you can come back to your jobs later.

# Quick Start

```{code-block} bash
# Run script.lua once for every line in granules.txt
sliderule-runner submit my_job script.lua granules.txt

# Check on progress
sliderule-runner status

# Summarize the results of completed jobs
sliderule-runner report

# Save the entries that did not succeed so they can be resubmitted
sliderule-runner scrape --output retry.txt
sliderule-runner submit my_job_retry script.lua retry.txt
```

where `granules.txt` contains one entry per line:

```{code-block} text
:caption: granules.txt

ATL03_20230601000000_10011901_006_01.h5
ATL03_20230601001000_10021901_006_01.h5
ATL03_20230602000000_10031901_006_01.h5
```

```{note}
Each line of the arguments file is passed to your script as one entry, and one result is produced for each entry. See [Job Results](#job-results) for what your script needs to return.
```

# Usage

```{code-block} text
sliderule-runner <command> [options]
```

```{list-table}
:header-rows: 1

* - Command
  - Purpose
* - [`submit`](#cmd-submit)
  - Submit a job
* - [`status`](#cmd-status)
  - Display the status of submitted jobs
* - [`report`](#runner-cmd-report)
  - Generate a report of completed jobs
* - [`scrape`](#cmd-scrape)
  - Generate a list of arguments from jobs with the provided result status
* - [`logs`](#cmd-logs)
  - Get log messages for a job
* - [`cancel`](#cmd-cancel)
  - Cancel a submitted job
* - [`archive`](#cmd-archive)
  - Save and clear the local database
```

Run `sliderule-runner --help` for a list of commands, or `sliderule-runner <command> --help` for the options of a specific command.

(common-options)=
# Common Options

The following options are accepted by **every** command. They are attached to each subcommand, so they must appear **after** the command name:

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--domain <domain>`
  - `slideruleearth.io`
  - Domain of the SlideRule service to connect to.
* - `--database <file>`
  - `~/.cache/sliderule/runner_database.json`
  - Location of the local database that tracks your submissions (see [The Job Database](#job-database)).
* - `--queue <priority>`
  - `default`
  - Priority queue that jobs are submitted to and looked up in. Valid values are `urgent`, `default`, and `background`. Used by the commands `submit`, `status` and `cancel`; accepted but ignored by the other commands.
* - `--verbose`
  - False
  - Turn on verbose log messages. Also shows more detail in `status` and `scrape`, and causes errors to be raised without being caught (see [Error Handling](#error-handling)).
```

```{note}
Every command connects to the SlideRule service and authenticates you before it runs, so network access is needed even for commands like `report` that work from the local database. Commands that read results (`status`) also need AWS credentials that can read the job output in S3.
```

---

# Command Reference

(cmd-submit)=
## `submit`
```{code-block} text
sliderule-runner submit <name> <script.lua> <arguments.txt> [options] [common options]
```

Submits a job that runs a script once for each entry in an arguments file. If the arguments file has more entries than `--batch_size`, the entries are split into multiple jobs, each of which is submitted and recorded separately.

```{list-table}
:header-rows: 1

* - Argument / Option
  - Default
  - Description
* - `<name>`
  - *(required)*
  - Base name for the submission. See [Job names](#job-names).
* - `<script file>`
  - *(required)*
  - Path to the script to run for each entry.
* - `<arguments file>`
  - *(required)*
  - Path to a file containing one entry per line. With `--arg_as_str` this is instead treated as a single entry.
* - `--arg_as_str`
  - False
  - Treat the arguments positional as a single string entry rather than the name of a file.
* - `--batch_size <int>`
  - `10000`
  - Maximum number of entries per job. Larger arguments files are split into multiple jobs.
* - `--vcpus <int>`
  - `4`
  - Number of virtual CPUs allocated to the job.
* - `--memory <int>`
  - `16000`
  - Memory in KBytes allocated to the job. (For example, 16000 is 16MB)
* - `--image <str>`
  - `sliderule:latest`
  - Container image used to run the job.
* - `--secrets <filename>`
  - `None`
  - File containing secrets to be passed to running script; must be formatted as json.
```

(job-names)=
### Job names

Each job is named `<name>_<i>`, where `<i>` is the position in the arguments file of the first entry in that batch. For example, submitting `my_job` with 25,000 entries and the default batch size creates `my_job_0`, `my_job_10000` and `my_job_20000`. If a name is already in your local database, three random letters are added (e.g. `my_job_xqf_0`) so that existing records are not overwritten.

```{important}
The other commands (`status` aside) refer to a submission by its full job name, such as `my_job_0`, not by the base name you supplied to `submit`. Job names are printed when the jobs are submitted, and `status` lists them.
```

### Secrets

Secrets live for at least 72 hours, after which they are lazily deleted; therefore, jobs that require secrets must complete within 72 hours.

### Examples
```{code-block} bash
# run script.lua on every entry in granules.txt
sliderule-runner submit my_job script.lua granules.txt

# split the entries into jobs of 500 and give each job more resources
sliderule-runner submit my_job script.lua granules.txt --batch_size 500 --vcpus 8 --memory 32000

# run the script on a single entry supplied on the command line
sliderule-runner submit one_off script.lua "ATL03_20230601000000_10011901_006_01.h5" --arg_as_str

# submit to a different queue using a custom image
sliderule-runner submit my_job script.lua granules.txt --queue <priority> --image sliderule:my-branch
```

(cmd-status)=
## `status`
```{code-block} text
sliderule-runner status [common options]
```

Checks on every submission in the local database that is not yet complete, updates the database, and prints a table of the current state of every submission. When all of the work in a submission has finished, the tool reads its results from S3 and marks the submission as complete, after which it is available to `report` and `scrape`.

Because the tool does not run in the background, `status` needs to be run again to pick up progress.

By default, the table has one row per submission and one column for each job state:

```{list-table}
:header-rows: 1

* - Column
  - Description
* - `NAME`
  - The full job name.
* - `SUBMITTED`, `PENDING`, `RUNNABLE`, `STARTING`, `RUNNING`
  - Number of jobs in each in-progress state.
* - `SUCCEEDED`, `FAILED`
  - Number of jobs that have finished in each state.
```

With `--verbose`, the table instead has one row per child job, showing the submission `NAME`, the child job `INDEX` and its `STATUS`. This index is the value to give to [`logs --index`](#cmd-logs).

```{admonition} Single Jobs
:class: attention
For runs that have only a single job, there are no child jobs and therefore the `--verbose` option is invalid and will result in an exception.
```

### Examples
```{code-block} bash
# check on all submitted jobs
sliderule-runner status

# show the status of each individual job in each submission
sliderule-runner status --verbose
```

(runner-cmd-report)=
## `report`
```{code-block} text
sliderule-runner report [--name <str>] [common options]
```

Generates a summary of the results of completed submissions. Submissions that are not yet complete are not included.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--name <str>`
  - none
  - Only report on the submission with this full job name.
```

The report has one row per completed submission with the following columns:

```{list-table}
:header-rows: 1

* - Column
  - Description
* - `name`
  - The full job name.
* - `processed`
  - Number of entries that have a recorded duration.
* - `exceptions`
  - Reserved; currently always 0.
* - `success`, `failure`, `unsupported`, `error`
  - Number of entries with each result status (see [Job Results](#job-results)).
* - `duration`
  - Average duration per processed entry, in seconds.
```

### Examples
```{code-block} bash
# report on all completed submissions
sliderule-runner report

# report on a single submission
sliderule-runner report --name my_job_0
```

(cmd-scrape)=
## `scrape`
```{code-block} text
sliderule-runner scrape [options] [common options]
```

Generates a list of the original entries whose results have one of the given statuses. By default it selects the entries that did not succeed, which makes it easy to build the arguments file for a resubmission. Submissions that are not yet complete are skipped.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--status <status...>`
  - `failure unsupported error`
  - One or more result statuses to select (see [Job Results](#job-results)).
* - `--name <str>`
  - none
  - Only scrape the submission with this full job name.
* - `--output <file>`
  - none
  - Write the selected entries to this file, one per line.
```

Each selected entry is printed with its position in the submission. With `--verbose`, the complete result record for each entry is printed (and written to `--output`) instead of just the entry.

### Examples
```{code-block} bash
# list everything that did not succeed across all completed submissions
sliderule-runner scrape

# save the entries that did not succeed to a file, ready to resubmit
sliderule-runner scrape --output retry.txt

# save only the entries that failed in a single submission
sliderule-runner scrape --name my_job_0 --status failure --output retry.txt
```

(cmd-logs)=
## `logs`
```{code-block} text
sliderule-runner logs --name <str> [--index <int>] [common options]
```

Prints the log messages for a job. Each message is printed on its own line, preceded by `->`.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--name <str>`
  - *(required)*
  - Full job name of the submission.
* - `--index <int>`
  - none
  - Index of a single child job within the submission, as shown by `status --verbose`. If omitted, the messages for the submission's parent job are printed.
```

### Examples
```{code-block} bash
# find the index of the job you are interested in
sliderule-runner status --verbose

# get the logs for child job 17 of the submission
sliderule-runner logs --name my_job_0 --index 17
```

```{note}
Run `status` at least once after submitting before requesting logs for a specific `--index`, since the child jobs are recorded in the local database by running the `status` command.
```

(cmd-cancel)=
## `cancel`
```{code-block} text
sliderule-runner cancel --name <str> [common options]
```

Cancels a submitted job. For a submission made up of many entries, this cancels the submission as a whole.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--name <str>`
  - *(required)*
  - Full job name of the submission to cancel.
```

```{note}
`cancel` does not update the local database; run `status` afterwards to see the effect.
```

### Examples
```{code-block} bash
sliderule-runner cancel --name my_job_0
```

(cmd-archive)=
## `archive`
```{code-block} text
sliderule-runner archive <full path to archive file> [common options]
```

Saves the contents of the local database to an archive file and then clears the database. Use it to start fresh once you are finished with a set of jobs.

```{list-table}
:header-rows: 1

* - Argument
  - Description
* - `<full path to archive file>`
  - *(required)* Where to save a copy of the database before it is cleared.
```

### Examples
```{code-block} bash
sliderule-runner archive ~/sliderule_archives/2026-09-runs.json
```

---

# Additional Topics

(job-database)=
### The Job Database

The tool keeps a JSON database of your submissions at `~/.cache/sliderule/runner_database.json` (change it with `--database`). For each submission it records what was returned when the job was submitted, the number of entries, the latest status, the child jobs, and, once the submission is complete, the results for every entry.

- The database is written when a command finishes successfully. If a command fails part way through, none of its changes are saved.
- Use [`archive`](#cmd-archive) to save a copy and start with an empty database.
- Use `--database` to keep separate sets of work in separate databases.

(job-results)=
### Job Results

When a submission finishes, the tool reads the output of each entry from S3. For this to work, the result your script writes for each entry must be a JSON object with the following fields:

```{code-block} json
{
  "status": true,
  "start": 1719849600.0,
  "stop": 1719849660.0,
  "outputs": ["<output1>", "<output2>"]
}
```

```{list-table}
:header-rows: 1

* - Field
  - Description
* - `status`
  - Boolean: whether the run for this entry succeeded.
* - `start`
  - Start time of the run, in seconds.
* - `stop`
  - Stop time of the run, in seconds.
* - `outputs`
  - List of the outputs produced for this entry.
```

Each entry is then given one of the following result statuses, which are the values used by `report` and by `scrape --status`:

```{list-table}
:header-rows: 1

* - Status
  - Meaning
* - `success`
  - The result was read and its `status` was true. Its duration is `stop - start`.
* - `failure`
  - The result was read and its `status` was false.
* - `unsupported`
  - The result was read, but it does not have the fields listed above, so it could not be interpreted.
* - `error`
  - The result could not be read.
```

```{note}
Run `sliderule-provisioner s3access` to get credentials to download results.
```

### Typical Workflow

```{code-block} bash
# 1. submit
sliderule-runner submit my_job script.lua granules.txt

# 2. monitor until everything is complete (repeat as needed)
sliderule-runner status

# 3. summarize
sliderule-runner report

# 4. investigate anything that went wrong
sliderule-runner status --verbose
sliderule-runner logs --name my_job_0 --index 17

# 5. collect what did not succeed and resubmit it
sliderule-runner scrape --output retry.txt
sliderule-runner submit my_job_retry script.lua retry.txt

# 6. when finished, archive the database
sliderule-runner archive ~/sliderule_archives/my_job.json
```

### Output Format

`status`, `report` and `scrape` print their results to standard output. `status` and `report` print tables of comma-separated, right-aligned columns with a header row. Progress bars are shown while results are being read if the optional `tqdm` package is installed; if it is not, a message is printed when the tool starts and progress is not reported.

(error-handling)=
### Error Handling

By default, errors are caught and reported as a single line followed by a traceback:

```{code-block} text
Unhandled error: <message>
```

Pass `--verbose` to raise the exception directly instead. When a command fails, the local database is not updated.
