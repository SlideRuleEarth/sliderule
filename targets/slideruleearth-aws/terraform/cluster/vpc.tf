resource "aws_vpc" "sliderule-vpc" {
    cidr_block           = var.vpcCIDRblock
    enable_dns_hostnames = true
    enable_dns_support   = true
    instance_tenancy     = "default"
    tags = {
      "Name" = "${var.cluster_name}-vpc"
    }
}

resource "aws_subnet" "sliderule-subnet" {
    vpc_id                  = aws_vpc.sliderule-vpc.id
    cidr_block              = "10.0.0.0/16"
    map_public_ip_on_launch = true
    availability_zone       = var.availability_zone
    tags = {
        Name = "${var.cluster_name}-subnet"
    }
}

resource "aws_internet_gateway" "sliderule-gateway" {
    vpc_id = aws_vpc.sliderule-vpc.id
    tags = {
        Name = "${var.cluster_name}-gateway"
    }
}

resource "aws_route_table" "sliderule-route" {
    vpc_id = aws_vpc.sliderule-vpc.id
    route {
        cidr_block = "0.0.0.0/0"
        gateway_id = aws_internet_gateway.sliderule-gateway.id
    }
    tags = {
        Name = "${var.cluster_name}-route"
    }
}

resource "aws_route_table_association" "sliderule-route-association"{
    subnet_id       = aws_subnet.sliderule-subnet.id
    route_table_id  = aws_route_table.sliderule-route.id
}
