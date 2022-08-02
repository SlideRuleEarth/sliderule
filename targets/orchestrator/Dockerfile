FROM python:3.10-slim-bullseye
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

# Install Python Scripts
COPY orchestrator.py /usr/local/etc/

# Start Orchestrator
CMD [ "python3", "/usr/local/etc/orchestrator.py"]