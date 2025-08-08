import sys
import json

with open(sys.argv[1]) as file:
    for line in file.readlines():
        entry = json.loads(line)
        timestamp = ''.join(i for i in entry["timestamp"] if ord(i)<128)
        msg = ''.join(i for i in entry["line"] if ord(i)<128)
        output = '{} {}'.format(timestamp, msg[1:])
        sys.stdout.write(output)
