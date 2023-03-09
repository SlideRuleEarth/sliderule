# install build dependencies
FROM haproxytech/haproxy-ubuntu:2.6

# configure haproxy
COPY haproxy.cfg /usr/local/etc/haproxy/haproxy.cfg
COPY *.lua /usr/local/etc/haproxy/

# set entrypoint to bash shell (expect it to be overridden)
# ENTRYPOINT ["/bin/sh"]
