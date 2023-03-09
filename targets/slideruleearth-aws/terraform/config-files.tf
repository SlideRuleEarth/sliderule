
resource "aws_s3_bucket_object" "docker-compose-config" {
  for_each = fileset("./", "docker-compose-*.yml")
  bucket = "sliderule"
  key = "infrastructure/software/${var.cluster_name}-${each.value}"
  source = "./${each.value}"
  etag = filemd5("./${each.value}")
}

resource "aws_s3_bucket_object" "nginx-config" {
  for_each = fileset("./", "*_nginx.service")
  bucket = "sliderule"
  key = "infrastructure/software/${var.cluster_name}-${each.value}"
  source = "./${each.value}"
  etag = filemd5("./${each.value}")
}

resource "aws_s3_bucket_object" "export-log-script" {
  for_each = fileset("./", "export_logs.sh")
  bucket = "sliderule"
  key = "infrastructure/software/${var.cluster_name}-${each.value}"
  source = "./${each.value}"
  etag = filemd5("./${each.value}")
}

resource "aws_s3_bucket_object" "cron-job" {
  for_each = fileset("./", "cronjob.txt")
  bucket = "sliderule"
  key = "infrastructure/software/${var.cluster_name}-${each.value}"
  source = "./${each.value}"
  etag = filemd5("./${each.value}")
}
