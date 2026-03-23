#!/bin/bash

# Retry function
retry() {
    local max_attempts=3
    local attempt=1
    local delay=5

    until "$@"; do
    if [ $attempt -eq $max_attempts ]; then
        echo "Command failed after $max_attempts attempts: $*"
        return 1
    fi
    echo "Attempt $attempt failed. Retrying in $delay seconds..."
    sleep $delay
    delay=$((delay * 2))
    ((attempt++))
    done
    return 0
}

# Install and configure docker
retry dnf update -y
retry dnf install -y docker
systemctl enable docker
systemctl start docker
usermod -aG docker ec2-user

# Install Docker Compose V2
retry curl -L "https://github.com/docker/compose/releases/download/v2.24.5/docker-compose-linux-aarch64" -o /usr/local/bin/docker-compose
chmod +x /usr/local/bin/docker-compose
mkdir -p /usr/local/lib/docker/cli-plugins
ln -s /usr/local/bin/docker-compose /usr/local/lib/docker/cli-plugins/docker-compose

# Log into ECR
aws ecr get-login-password --region $AWS_REGION | docker login --username AWS --password-stdin $CONTAINER_REGISTRY

# Setup environment
export MYIP=$(hostname -I | awk '{print $1}')

# Download AMS files
mkdir -p /data
aws s3 cp s3://$PROJECT_BUCKET/$PROJECT_FOLDER/ams/ /data/ --recursive

# Download plugin files
mkdir -p /plugins
aws s3 cp s3://$PROJECT_BUCKET/plugins/ /plugins/ --recursive

# Create docker-compose.yml
cat > docker-compose.yml << EOF
version: "3.9"
services:
  sliderule:
    image: $CONTAINER_REGISTRY/sliderule:$VERSION
    container_name: sliderule
    network_mode: host
    restart: always
    pull_policy: if_not_present
    command: ["/usr/local/etc/sliderule/server.lua"]
    ulimits:
      nofile:
        soft: 8192
        hard: 8192
    volumes:
      - /etc/ssl/certs:/etc/ssl/certs
      - /var/run/docker.sock:/var/run/docker.sock
      - /data:/data
      - /plugins:/plugins
    environment:
      - LOG_FORMAT=FMT_CLOUD
      - IPV4=$MYIP
      - ENVIRONMENT_VERSION=$ENVIRONMENT_VERSION
      - PROJECT_BUCKET=$PROJECT_BUCKET
      - PROJECT_FOLDER=$PROJECT_FOLDER
      - PROJECT_REGION=$AWS_REGION
      - ORCHESTRATOR=http://10.0.128.5:8050
      - ALERT_STREAM=$ALERT_STREAM
      - TELEMETRY_STREAM=$TELEMETRY_STREAM
      - CLUSTER=$CLUSTER
      - DOMAIN=$DOMAIN
      - AMS=http://127.0.0.1:9082
      - CONTAINER_REGISTRY=$CONTAINER_REGISTRY
    healthcheck:
      test: curl -f -X GET -d "{}" http://localhost:9081/source/health
      interval: 30s
      timeout: 10s
      retries: 1
      start_period: 30s
    labels:
      - autoheal=true
  ams:
    image: $CONTAINER_REGISTRY/ams:$VERSION
    container_name: ams
    network_mode: host
    restart: always
    pull_policy: if_not_present
    entrypoint: /docker-entrypoint.sh
    volumes:
      - /data:/data
    labels:
      - autoheal=true
  monitor-agent:
    image: $CONTAINER_REGISTRY/monitor-agent:$VERSION
    container_name: monitor-agent
    network_mode: host
    restart: unless-stopped
    pull_policy: if_not_present
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
      - /:/rootfs:ro
      - /var/log:/var/log:ro
      - /var/log/journal:/var/log/journal:ro
      - /run/log/journal:/run/log/journal:ro
      - /etc/machine-id:/etc/machine-id:ro
      - /var/lib/docker/containers:/var/lib/docker/containers:ro
      - /var/run/docker.sock:/var/run/docker.sock
    labels:
      - autoheal=true
  autoheal:
    image: willfarrell/autoheal:latest
    container_name: autoheal
    restart: always
    environment:
      - AUTOHEAL_CONTAINER_LABEL=all
      - AUTOHEAL_INTERVAL=30
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
EOF

# Start services
docker compose -p cluster up --detach
