#!/bin/bash

# AWS Configuration
AWS_REGION="eu-north-1"
S3_BUCKET="noise-mapping-audio"
DYNAMODB_TABLE="NoiseClassification"
IAM_ROLE="NoiseMappingEC2Role"
INSTANCE_TYPE="t2.micro"
INSTANCE_PROFILE="NoiseMappingEC2Profile"
KEY_NAME="noise-mapping-key"
SECURITY_GROUP="NoiseMappingSG"
AMI_ID="ami-0c66be8c9de421368"  # Amazon Linux 2 AMI for eu-north-1

echo "🔹 Setting up AWS resources in region: $AWS_REGION"

# 1️⃣ Create S3 Bucket
echo "🔹 Creating S3 bucket: $S3_BUCKET..."
if aws s3 ls "s3://$S3_BUCKET" 2>/dev/null; then
    echo "S3 bucket $S3_BUCKET already exists."
else
    echo "Creating S3 bucket: $S3_BUCKET..."
    aws s3 mb s3://$S3_BUCKET --region $AWS_REGION
fi

# 2️⃣ Create DynamoDB Table
echo "🔹 Creating DynamoDB table: $DYNAMODB_TABLE..."
if aws dynamodb describe-table --table-name $DYNAMODB_TABLE --region $AWS_REGION 2>/dev/null; then
    echo "DynamoDB table $DYNAMODB_TABLE already exists."
else
    echo "Creating DynamoDB table: $DYNAMODB_TABLE..."
    aws dynamodb create-table \
        --table-name $DYNAMODB_TABLE \
        --attribute-definitions AttributeName=device_id,AttributeType=S \
        --key-schema AttributeName=device_id,KeyType=HASH \
        --billing-mode PAY_PER_REQUEST \
        --region $AWS_REGION
fi

# 3️⃣ Create IAM Role for EC2
echo "🔹 Creating IAM Role: $IAM_ROLE..."
if aws iam get-role --role-name $IAM_ROLE 2>/dev/null; then
    echo "IAM role $IAM_ROLE already exists."
else
    echo "Creating IAM Role: $IAM_ROLE..."
    aws iam create-role \
        --role-name $IAM_ROLE \
        --assume-role-policy-document file://trust-policy.json
fi

# Attach policies for S3, DynamoDB, and Lambda
aws iam attach-role-policy --role-name $IAM_ROLE --policy-arn arn:aws:iam::aws:policy/AmazonS3FullAccess
aws iam attach-role-policy --role-name $IAM_ROLE --policy-arn arn:aws:iam::aws:policy/AmazonDynamoDBFullAccess
aws iam attach-role-policy --role-name $IAM_ROLE --policy-arn arn:aws:iam::aws:policy/AWSLambdaFullAccess
# 4️⃣ Create Security Group
echo "🔹 Creating Security Group: $SECURITY_GROUP..."
SECURITY_GROUP_ID=$(aws ec2 describe-security-groups \
    --query "SecurityGroups[?GroupName=='$SECURITY_GROUP'].GroupId" \
    --output text --region $AWS_REGION)

if [ -z "$SECURITY_GROUP_ID" ]; then
    echo "Creating Security Group: $SECURITY_GROUP..."
    SECURITY_GROUP_ID=$(aws ec2 create-security-group \
        --group-name $SECURITY_GROUP \
        --description "Allow SSH and HTTP" \
        --vpc-id $(aws ec2 describe-vpcs --query 'Vpcs[0].VpcId' --output text) \
        --region $AWS_REGION --query 'GroupId' --output text)
else
    echo "Security group $SECURITY_GROUP already exists."
fi

# Allow SSH and HTTP access
if ! aws ec2 describe-security-groups --group-ids $SECURITY_GROUP_ID --query "SecurityGroups[0].IpPermissions[?FromPort==\`22\`]" --output text | grep -q "22"; then
    aws ec2 authorize-security-group-ingress --group-id $SECURITY_GROUP_ID --protocol tcp --port 22 --cidr 0.0.0.0/0
else
    echo "SSH rule already exists."
fi

if ! aws ec2 describe-security-groups --group-ids $SECURITY_GROUP_ID --query "SecurityGroups[0].IpPermissions[?FromPort==\`8080\`]" --output text | grep -q "8080"; then
    aws ec2 authorize-security-group-ingress --group-id $SECURITY_GROUP_ID --protocol tcp --port 8080 --cidr 0.0.0.0/0
else
    echo "HTTP rule already exists."
fi

# 5️⃣ Create EC2 Instance
echo "🔹 Launching EC2 instance..."
if aws ec2 describe-key-pairs --key-names $KEY_NAME --region $AWS_REGION 2>/dev/null; then
    echo "Key pair $KEY_NAME already exists."
else
    echo "Creating EC2 Key Pair: $KEY_NAME..."
    aws ec2 create-key-pair --key-name $KEY_NAME --region $AWS_REGION \
        --query "KeyMaterial" --output text > $KEY_NAME.pem
    chmod 400 $KEY_NAME.pem
fi

INSTANCE_ID=$(aws ec2 run-instances \
    --image-id $AMI_ID \
    --count 1 \
    --instance-type $INSTANCE_TYPE \
    --key-name $KEY_NAME \
    --security-group-ids $SECURITY_GROUP_ID \
    --iam-instance-profile Name=$INSTANCE_PROFILE \
    --region $AWS_REGION \
    --query 'Instances[0].InstanceId' --output text)

echo "🔹 Waiting for EC2 instance to start..."
aws ec2 wait instance-running --instance-ids $INSTANCE_ID

# Get Public IP
EC2_PUBLIC_IP=$(aws ec2 describe-instances \
    --instance-ids $INSTANCE_ID \
    --query 'Reservations[0].Instances[0].PublicIpAddress' --output text)

echo "✅ EC2 instance launched! Connect using:"
echo "ssh -i $KEY_NAME.pem ec2-user@$EC2_PUBLIC_IP"
echo "✅ Run your Go API on: http://$EC2_PUBLIC_IP:8080"