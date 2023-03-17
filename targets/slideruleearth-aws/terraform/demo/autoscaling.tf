resource "aws_iam_role" "autoscaling" {
  name               = "${var.domain_root}-${var.Name_tag}-appautoscaling-role"
  assume_role_policy = file("policies/appautoscaling-role.json")
}

resource "aws_iam_role_policy" "autoscaling" {
  name   = "${var.domain_root}-${var.Name_tag}-appautoscaling-policy"
  policy = file("policies/appautoscaling-role-policy.json")
  role   = aws_iam_role.autoscaling.id
}

resource "aws_appautoscaling_target" "ecs_target" {
  max_capacity       = "${var.auto_scale_max}"
  min_capacity       = "${var.auto_scale_min}"
  resource_id        = "service/${aws_ecs_cluster.demo.name}/${aws_ecs_service.demo-ecs-service.name}"
  role_arn           = aws_iam_role.autoscaling.arn
  scalable_dimension = "ecs:service:DesiredCount"
  service_namespace  = "ecs"
  depends_on = [aws_ecs_service.demo-ecs-service]
}

resource "aws_appautoscaling_policy" "ecs_mem_tgt_policy" {
  name               = "${var.domain_root}-${var.Name_tag}-mem-asp"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.ecs_target.resource_id
  scalable_dimension = aws_appautoscaling_target.ecs_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.ecs_target.service_namespace

  target_tracking_scaling_policy_configuration {
    target_value = "${var.mem_scale_target_value}"
    scale_in_cooldown  = "${var.mem_scale_in_cooldown}"
    scale_out_cooldown = "${var.mem_scale_out_cooldown}"
    predefined_metric_specification {
      predefined_metric_type = "ECSServiceAverageMemoryUtilization"
    }
  }
  depends_on = [aws_appautoscaling_target.ecs_target]
}

resource "aws_appautoscaling_policy" "ecs_cpu_tgt_policy" {
  name               = "${var.domain_root}-${var.Name_tag}-cpu-asp"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.ecs_target.resource_id
  scalable_dimension = aws_appautoscaling_target.ecs_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.ecs_target.service_namespace

  target_tracking_scaling_policy_configuration {
    target_value = "${var.cpu_scale_target_value}"
    scale_in_cooldown  = "${var.cpu_scale_in_cooldown}"
    scale_out_cooldown = "${var.cpu_scale_out_cooldown}"
    predefined_metric_specification {
      predefined_metric_type = "ECSServiceAverageCPUUtilization"
    }
  }
  #create the policies one at at time
  depends_on = [aws_appautoscaling_target.ecs_target,aws_appautoscaling_policy.ecs_mem_tgt_policy]
}

# resource "aws_appautoscaling_policy" "ecs_req_cnt_tgt_policy" {
#   name               = "${var.domain_root}-${var.Name_tag}-cpu-asp"
#   policy_type        = "TargetTrackingScaling"
#   resource_id        = aws_appautoscaling_target.ecs_target.resource_id
#   scalable_dimension = aws_appautoscaling_target.ecs_target.scalable_dimension
#   service_namespace  = aws_appautoscaling_target.ecs_target.service_namespace

#   target_tracking_scaling_policy_configuration {
#     target_value = "${var.req_cnt_scale_target_value}"
#     scale_in_cooldown  = "${var.req_cnt_scale_in_cooldown}"
#     scale_out_cooldown = "${var.req_cnt_scale_out_cooldown}"
#     predefined_metric_specification {
#       predefined_metric_type = "ALBRequestCountPerTarget"
#       // app/<load-balancer-name>/<load-balancer-id>/targetgroup/<target-group-name>/<target-group-id>
#       resource_label = "${aws_lb.demo.arn_suffix}/${aws_alb_target_group.demo-target-group.arn_suffix}"
#     }
#   }
#   depends_on = [aws_appautoscaling_target.ecs_target]
# }
