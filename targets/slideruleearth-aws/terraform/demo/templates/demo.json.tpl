[
    {
        "name": "${domain_root}-${Name_tag}",
        "image": "${docker_image_url_demo-client}",
        "essential": true,
        "networkMode": "awsvpc",
        "entrypoint" : ["/usr/local/etc/docker-entrypoint.sh"],
        "portMappings": [
            {
                "containerPort": ${demo_container_port},
                "protocol": "tcp"
            }
        ],
        "environment": [
            {
                "name": "GRPC_POLL_STRATEGY",
                "value": "poll"
            }
        ],
        "logConfiguration": {
            "logDriver": "awslogs",
            "options": {
                "awslogs-create-group": "true",
                "awslogs-group": "/ecs/${domain_root}-${Name_tag}",
                "awslogs-region": "${region}",
                "awslogs-stream-prefix": "${Name_tag}-voila"
            }
        }
    },
    {
        "name": "${domain_root}-${Name_tag}-nginx",
        "image": "${docker_image_url_demo-nginx}",
        "essential": true,
        "networkMode": "awsvpc",
        "portMappings": [
            {
                "containerPort": ${nginx_container_port},
                "protocol": "tcp"
            }
        ],
        "dependsOn": [
            {
            "containerName": "${domain_root}-${Name_tag}",
            "condition": "START"
            }
        ],
        "logConfiguration": {
            "logDriver": "awslogs",
            "options": {
                "awslogs-create-group": "true",
                "awslogs-group": "/ecs/${domain_root}-${Name_tag}-nginx",
                "awslogs-region": "${region}",
                "awslogs-stream-prefix": "${domain_root}-${Name_tag}-nginx"
            }
        }
    }
]