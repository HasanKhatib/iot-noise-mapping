#!/bin/bash

AWS_REGION="eu-west-1"
ECR_REPO="noise-mapping-service"
IAM_ROLE="AppRunnerECRAccessRole"
AWS_ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)

echo "🚀 Deploying FastAPI service to AWS App Runner..."

# 1️⃣ **Navigate to the Backend Directory (where Dockerfile is located)**
cd backend || { echo "❌ Error: backend directory not found!"; exit 1; }

# 2️⃣ **Authenticate Docker with AWS ECR**
aws ecr get-login-password --region $AWS_REGION | docker login --username AWS --password-stdin $AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com

# 3️⃣ **Build & Push Docker Image to AWS ECR**
docker build --no-cache --platform=linux/amd64 -t $ECR_REPO .
docker tag $ECR_REPO:latest $AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com/$ECR_REPO:latest
docker push $AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com/$ECR_REPO:latest
echo "✅ Docker image pushed to AWS ECR."

# 4️⃣ **Navigate back to the root directory**
cd ..

# 5️⃣ **Create IAM Role for App Runner (if not exists)**
if aws iam get-role --role-name $IAM_ROLE 2>/dev/null; then
    echo "✅ IAM role $IAM_ROLE already exists."
else
    aws iam create-role --role-name $IAM_ROLE \
        --assume-role-policy-document file://<(cat <<EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Principal": {"Service": "build.apprunner.amazonaws.com"},
            "Action": "sts:AssumeRole"
        }
    ]
}
EOF
    )
    aws iam attach-role-policy --role-name $IAM_ROLE \
        --policy-arn arn:aws:iam::aws:policy/AmazonEC2ContainerRegistryPowerUser
    echo "✅ IAM Role for App Runner created."
fi

# 6️⃣ **Deploy or Update App Runner Service**
SERVICE_NAME="noise-mapping"
SERVICE_EXISTS=$(aws apprunner list-services --query "ServiceSummaryList[?ServiceName=='$SERVICE_NAME'].ServiceName" --output text --region $AWS_REGION)

if [ "$SERVICE_EXISTS" == "$SERVICE_NAME" ]; then
    SERVICE_ARN=$(aws apprunner list-services --query "ServiceSummaryList[?ServiceName=='$SERVICE_NAME'].ServiceArn" --output text --region $AWS_REGION)
    if [ "$SERVICE_STATUS" == "RUNNING" ]; then
        aws apprunner start-deployment --service-arn $SERVICE_ARN --region $AWS_REGION
        echo "✅ AWS App Runner service updated!"
    else
        echo "❌ Service is not in RUNNING state (current state: $SERVICE_STATUS). Trying to fix..."
        if [ "$SERVICE_STATUS" == "PAUSED" ]; then
            aws apprunner resume-service --service-arn $SERVICE_ARN --region $AWS_REGION
            echo "✅ Service resumed. Wait for it to be RUNNING before retrying deployment."
        else
            echo "❌ Service is in an invalid state ($SERVICE_STATUS). Consider recreating it."
        fi
    fi
    echo "✅ AWS App Runner service updated!"
else
    aws apprunner create-service \
        --service-name $SERVICE_NAME \
        --source-configuration "{\"ImageRepository\":{\"ImageIdentifier\":\"$AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com/$ECR_REPO:latest\",\"ImageRepositoryType\":\"ECR\",\"ImageConfiguration\":{\"Port\":\"8080\"}},\"AuthenticationConfiguration\":{\"AccessRoleArn\":\"arn:aws:iam::$AWS_ACCOUNT_ID:role/$IAM_ROLE\"}}" \
        --instance-configuration "{\"Cpu\":\"1 vCPU\",\"Memory\":\"2 GB\"}" \
        --region $AWS_REGION
    echo "✅ AWS App Runner deployment started! Monitor status with:"
    echo "aws apprunner list-services --region $AWS_REGION"
fi
