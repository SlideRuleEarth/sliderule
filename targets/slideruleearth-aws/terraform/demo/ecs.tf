resource "aws_ecs_cluster" "demo" {
  name = "${var.domain_root}-${var.Name_tag}-ecs-clstr"
  setting {
    name  = "containerInsights"
    value = "enabled"
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-ecs-clstr"
  }
}

resource "aws_ecs_cluster_capacity_providers" "demo" {
  cluster_name = aws_ecs_cluster.demo.name

  capacity_providers = ["FARGATE"]

  default_capacity_provider_strategy {
    base              = 1
    weight            = 100
    capacity_provider = "FARGATE"
  }
}

data "template_file" "demo" {
  template = file("templates/demo.json.tpl")

  vars = {
    docker_image_url_demo-client  = var.docker_image_url_demo-client
    docker_image_url_demo-nginx   = var.docker_image_url_demo-nginx
    region                        = var.region
    demo_container_port           = var.demo_container_port
    nginx_container_port          = var.nginx_container_port
    Name_tag                      = var.Name_tag
    domain_root                   = var.domain_root
  }
}

resource "aws_ecs_task_definition" "demo" {
  family                    = "${var.domain_root}-${var.Name_tag}"
  requires_compatibilities  = ["FARGATE"]
  network_mode              = "awsvpc"
  cpu                       = var.demo_task_cpu
  memory                    = var.demo_task_memory
  execution_role_arn        = aws_iam_role.tasks-service-role.arn
  task_role_arn             = aws_iam_role.ecs_task_role.arn
  container_definitions     = data.template_file.demo.rendered
  runtime_platform {
    operating_system_family = "LINUX"
    cpu_architecture = var.runtime_cpu_arch
  }

  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-ecs-task"
  }
}

resource "aws_ecs_service" "demo-ecs-service" {
  name            = "${var.domain_root}-${var.Name_tag}-ecs-srvc"
  cluster         = aws_ecs_cluster.demo.id
  task_definition = aws_ecs_task_definition.demo.arn
  desired_count   = var.task_count
  launch_type     = "FARGATE"
  depends_on      = [aws_alb_listener.demo-http-listener]

  network_configuration {
    security_groups = [aws_security_group.task-sg.id]
    subnets         = aws_subnet.private.*.id
  }

  load_balancer {
    target_group_arn = aws_alb_target_group.demo-target-group.arn
    container_name   = "${var.domain_root}-${var.Name_tag}-nginx"
    container_port   = var.nginx_container_port
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-ecs-srvc"
  }

  lifecycle {
    ignore_changes = [desired_count]
  }
}
