[
    {
        "name": "${domain_root}-${Name_tag}",
        "image": "${docker_image_url_static-website}",
        "networkMode": "awsvpc",
        "portMappings": [
        {
            "containerPort": ${container_port},
            "protocol": "tcp"
        }
        ],
        "logConfiguration": {
            "logDriver": "awslogs",
            "options": {
                "awslogs-create-group": "true",
                "awslogs-group": "/ecs/${domain_root}-${Name_tag}",
                "awslogs-region": "${region}",
                "awslogs-stream-prefix": "${domain_root}-${Name_tag}-ecs"
            }
        }
    }
]