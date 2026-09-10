# {
#     "submissions": {
#         "<name>": {
#             "run_url": <run url>,
#             "job_id": <job id>,
#             "status": {
#                 "SUBMITTED": <x>,
#                 "PENDING": <x>,
#                 "RUNNABLE": <x>,
#                 "STARTING": <x>,
#                 "RUNNING": <x>,
#                 "SUCCEEDED": <x>,
#                 "FAILED": <x>
#             },
#             "complete": <true|false>
#         },
#         ...
#     }
# }

import json
import os
from enum import Enum

# ###############################
# JobState
# ###############################

class JobState(str, Enum):

    SUBMITTED   = "SUBMITTED"
    PENDING     = "PENDING"
    RUNNABLE    = "RUNNABLE"
    STARTING    = "STARTING"
    RUNNING     = "RUNNING"
    SUCCEEDED   = "SUCCEEDED"
    FAILED      = "FAILED"

    def __str__(self):
        return self.value

# ###############################
# QueuePriority
# ###############################

class QueuePriority(str, Enum):

    URGENT      = "urgent"
    DEFAULT     = "default"
    BACKGROUND  = "background"

    def __str__(self):
        return self.value

# ###############################
# Database
# ###############################

class Database:

    def __init__(self, filename):
        self.filename = filename
        try:
            # read database
            with open(filename, "r") as file:
                self.database = json.load(file)
        except FileNotFoundError:
            # create database
            os.makedirs(os.path.dirname(filename), exist_ok=True)
            with open(filename, "w") as file:
                self.database = {"submissions": {}}
                json.dump(self.database, file)

    @property
    def submissions(self):
        return self.database["submissions"]

    # Write database out to file
    def write(self, filename=None):
        filename = filename or self.filename
        # written via a temporary file so that an interrupt cannot truncate the database
        tmp_filename = f"{filename}.tmp"
        with open(tmp_filename, "w") as file:
            json.dump(self.database, file, indent=2)
        os.replace(tmp_filename, filename)
