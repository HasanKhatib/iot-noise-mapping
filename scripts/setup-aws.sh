#!/bin/bash

AWS_REGION="eu-west-1"  # App Runner is NOT in eu-north-1, so we use eu-west-1
S3_BUCKET="iot-noise-mapping-audio"
DYNAMODB_TABLE="NoiseClassification"
IAM_USER="iot-noise-mapping-user"
IAM_POLICY="IOTNoiseMappingPolicy"
ECR_REPO="noise-mapping-service"

echo "🚀 Setting up AWS resources in region: $AWS_REGION"

# 1️⃣ **Create S3 Bucket**
if aws s3 ls "s3://$S3_BUCKET" --region $AWS_REGION 2>/dev/null; then
    echo "✅ S3 bucket $S3_BUCKET already exists."
else
    aws s3 mb "s3://$S3_BUCKET" --region $AWS_REGION
    echo "✅ S3 bucket created!"
fi

# 2️⃣ **Create DynamoDB Table**
if aws dynamodb describe-table --table-name $DYNAMODB_TABLE --region $AWS_REGION 2>/dev/null; then
    echo "✅ DynamoDB table $DYNAMODB_TABLE already exists."
else
    aws dynamodb create-table \
        --table-name $DYNAMODB_TABLE \
        --attribute-definitions AttributeName=device_id,AttributeType=S \
        --key-schema AttributeName=device_id,KeyType=HASH \
        --billing-mode PAY_PER_REQUEST \
        --region $AWS_REGION
    echo "✅ DynamoDB table created!"
fi

# 3️⃣ **Create IAM User**
if aws iam get-user --user-name $IAM_USER 2>/dev/null; then
    echo "✅ IAM user $IAM_USER already exists."
else
    aws iam create-user --user-name $IAM_USER
    echo "✅ IAM user created!"
fi

# 4️⃣ **Create IAM Policy**
if aws iam list-policies --query "Policies[?PolicyName=='$IAM_POLICY']" --output json | grep -q "$IAM_POLICY"; then
    echo "✅ IAM policy $IAM_POLICY already exists."
else
    POLICY_ARN=$(aws iam create-policy \
        --policy-name $IAM_POLICY \
        --policy-document file://<(cat <<EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": ["s3:PutObject", "s3:GetObject", "s3:ListBucket"],
            "Resource": ["arn:aws:s3:::$S3_BUCKET", "arn:aws:s3:::$S3_BUCKET/*"]
        },
        {
            "Effect": "Allow",
            "Action": ["dynamodb:PutItem", "dynamodb:Scan", "dynamodb:Query"],
            "Resource": "arn:aws:dynamodb:$AWS_REGION:*:table/$DYNAMODB_TABLE"
        }
    ]
}
EOF
    ) --query "Policy.Arn" --output text)
    echo "✅ IAM policy created: $POLICY_ARN"
fi

# 5️⃣ **Attach Policy to IAM User**
aws iam attach-user-policy --user-name $IAM_USER --policy-arn $POLICY_ARN
echo "✅ IAM policy attached!"

# 6️⃣ **Create AWS ECR Repository**
if aws ecr describe-repositories --repository-names $ECR_REPO --region $AWS_REGION 2>/dev/null; then
    echo "✅ ECR repository $ECR_REPO already exists."
else
    aws ecr create-repository --repository-name $ECR_REPO --region $AWS_REGION
    echo "✅ ECR repository created!"
fi

echo "✅ AWS setup complete! You can now run the service locally OR deploy it."