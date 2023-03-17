resource "aws_ecs_cluster" "static-website" {
  name = "${var.domain_root}-${var.Name_tag}-ecs-clstr"
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-ecs-clstr"
  }
}

resource "aws_ecs_cluster_capacity_providers" "static-website" {
  cluster_name = aws_ecs_cluster.static-website.name

  capacity_providers = ["FARGATE"]

  default_capacity_provider_strategy {
    base              = 1
    weight            = 100
    capacity_provider = "FARGATE"
  }
}

data "template_file" "static-website" {
  template = file("templates/static-website.json.tpl")

  vars = {
    docker_image_url_static-website = var.docker_image_url_static-website
    region                          = var.region
    container_port                  = var.container_port
    Name_tag                        = var.Name_tag
    domain_root                     = var.domain_root
  }
}

resource "aws_ecs_task_definition" "static-website" {
  family                    = "${var.domain_root}-${var.Name_tag}"
  requires_compatibilities  = ["FARGATE"]
  network_mode              = "awsvpc"
  cpu                       = var.fargate_task_cpu
  memory                    = var.fargate_task_memory
  execution_role_arn        = aws_iam_role.tasks-service-role.arn
  task_role_arn             = aws_iam_role.ecs_task_role.arn
  container_definitions     = data.template_file.static-website.rendered
  runtime_platform {
    operating_system_family = "LINUX"
    cpu_architecture = var.runtime_cpu_arch
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-ecs-task"
  }
}

resource "aws_ecs_service" "static-website-ecs-service" {
  name            = "${var.domain_root}-${var.Name_tag}-ecs-srvc"
  cluster         = aws_ecs_cluster.static-website.id
  task_definition = aws_ecs_task_definition.static-website.arn
  desired_count   = var.task_count
  launch_type     = "FARGATE"
  depends_on      = [aws_alb_listener.static-website-http-listener]

  network_configuration {
    security_groups = [aws_security_group.task-sg.id]
    subnets         = aws_subnet.private.*.id
  }

  load_balancer {
    target_group_arn = aws_alb_target_group.static-website-target-group.arn
    container_name   = "${var.domain_root}-${var.Name_tag}"
    container_port   = var.container_port
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-ecs-srvc"
  }
}
