resource "aws_instance" "manager" {
    ami                         = data.aws_ami.sliderule_cluster_ami.id
    availability_zone           = var.availability_zone
    ebs_optimized               = false
    instance_type               = "c7g.large"
    monitoring                  = false
    key_name                    = var.key_pair_name
    vpc_security_group_ids      = [aws_security_group.manager-sg.id]
    subnet_id                   = aws_subnet.sliderule-subnet.id
    associate_public_ip_address = true
    source_dest_check           = true
    iam_instance_profile        = aws_iam_instance_profile.s3-role.name
    private_ip                  = var.manager_ip
    root_block_device {
      volume_type               = "gp2"
      volume_size               = 40
      delete_on_termination     = true
    }
    tags = {
      "Name" = "${var.cluster_name}-manager"
    }
    user_data = <<-EOF
      #!/bin/bash
      echo ${var.cluster_name} > ./clustername.txt
      aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin ${var.container_repo}
      export DOMAIN=${var.domain}
      export MANAGER_SECRET_SALT='${local.secrets.manager_secret_salt}'
      export DUCKDB_LOCAL_FILE='/data/manager-${var.cluster_name}.db'
      export DUCKDB_REMOTE_FILE='s3://sliderule/config/manager-${var.cluster_name}.db'
      export MANAGER_IMAGE=${var.container_repo}/manager:${var.cluster_version}
      mkdir /data
      aws s3 cp $DUCKDB_REMOTE_FILE $DUCKDB_LOCAL_FILE || true
      aws s3 cp s3://sliderule/config/GeoLite2-ASN.mmdb /data/GeoLite2-ASN.mmdb
      aws s3 cp s3://sliderule/config/GeoLite2-City.mmdb /data/GeoLite2-City.mmdb
      aws s3 cp s3://sliderule/config/GeoLite2-Country.mmdb /data/GeoLite2-Country.mmdb
      aws s3 cp s3://sliderule/infrastructure/software/${var.cluster_name}-docker-compose-manager.yml ./docker-compose.yml
      docker-compose -p cluster up --detach
    EOF
}
