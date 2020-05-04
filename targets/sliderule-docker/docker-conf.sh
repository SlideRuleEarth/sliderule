# Docker needs to be able to resolve DNS, and by default uses Google's DNS server.
# If that server is not available or blocked by the local network, then the following
# steps are needed:
#
# Step 1: Determine local DNS server
#
# Step 2: Copy that into the daemon.json file as the first entry
#
# Step 3: Run this script to copy daemon.json to the correct directory and
#         restart the docker service 

# nmcli dev show | grep 'IP4.DNS'
cp daemon.json /etc/docker
service docker restart

