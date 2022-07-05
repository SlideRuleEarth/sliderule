FROM 3.10-slim-bullseye
MAINTAINER JP Swinski (jp.swinski@nasa.gov)

# Install Python Scripts
COPY orchestrator.py /usr/local/etc/

# Copy Over Entry Point Script
COPY docker-entrypoint.sh /usr/local/etc/

# Default Entry Point (likely overridden by command line invocation of entrypoint script above)
ENTRYPOINT ["/usr/bin/bash"]
