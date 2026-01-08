# AWS Production Deployment Guide

This guide covers deploying the IoT Noise Mapping system to AWS using App Runner, S3, and DynamoDB.

## Prerequisites

- AWS Account with appropriate permissions
- AWS CLI installed and configured: `aws configure`
- Docker installed
- Permissions to create:
  - S3 buckets
  - DynamoDB tables
  - ECR repositories
  - App Runner services
  - IAM roles and policies

## Architecture Overview

```
ESP32 Devices → App Runner (FastAPI) → S3 (Audio Storage)
                        ↓
                  DynamoDB (Metadata)
```

## Step 1: Configure AWS CLI

If you haven't already, configure your AWS credentials:

```bash
aws configure
```

Enter your:
- AWS Access Key ID
- AWS Secret Access Key
- Default region (e.g., `eu-west-1` or `us-east-1`)
- Default output format: `json`

Verify configuration:
```bash
aws sts get-caller-identity
```

## Step 2: Set Up AWS Infrastructure

Run the automated setup script to create all required AWS resources:

```bash
chmod +x scripts/setup-aws.sh
./scripts/setup-aws.sh
```

This script will:
1. ✅ Create S3 bucket: `iot-noise-mapping-audio`
2. ✅ Create DynamoDB table: `NoiseClassification`
3. ✅ Create ECR repository: `noise-mapping-service`
4. ✅ Create IAM user: `noise-mapping-user` with appropriate policies
5. ✅ Generate and store access credentials

### Manual Setup (Alternative)

If you prefer manual setup or the script fails, run these commands:

```bash
# Set your AWS region
export AWS_REGION="eu-west-1"

# Create S3 bucket
aws s3 mb s3://iot-noise-mapping-audio --region $AWS_REGION

# Create DynamoDB table
aws dynamodb create-table \
    --table-name NoiseClassification \
    --attribute-definitions \
        AttributeName=UUID,AttributeType=S \
    --key-schema \
        AttributeName=UUID,KeyType=HASH \
    --billing-mode PAY_PER_REQUEST \
    --region $AWS_REGION

# Create ECR repository
aws ecr create-repository \
    --repository-name noise-mapping-service \
    --region $AWS_REGION
```

## Step 3: Build and Push Docker Image

Get your AWS account ID:

```bash
export AWS_ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)
export AWS_REGION="eu-west-1"  # or your preferred region
```

Authenticate Docker with ECR:

```bash
aws ecr get-login-password --region $AWS_REGION | \
    docker login --username AWS --password-stdin \
    $AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com
```

Build the Docker image:

```bash
cd backend
docker build -t noise-mapping-service .
```

Tag and push to ECR:

```bash
docker tag noise-mapping-service:latest \
    $AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com/noise-mapping-service:latest

docker push $AWS_ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com/noise-mapping-service:latest
```

## Step 4: Deploy to AWS App Runner

Run the deployment script:

```bash
chmod +x scripts/deploy.sh
./scripts/deploy.sh
```

Or manually create the App Runner service:

```bash
aws apprunner create-service \
    --service-name noise-mapping-service \
    --source-configuration '{
        "ImageRepository": {
            "ImageIdentifier": "'$AWS_ACCOUNT_ID'.dkr.ecr.'$AWS_REGION'.amazonaws.com/noise-mapping-service:latest",
            "ImageRepositoryType": "ECR",
            "ImageConfiguration": {
                "Port": "8080",
                "RuntimeEnvironmentVariables": {
                    "AWS_REGION": "'$AWS_REGION'",
                    "S3_BUCKET": "iot-noise-mapping-audio",
                    "DYNAMODB_TABLE": "NoiseClassification",
                    "MODEL_TYPE": "yamnet"
                }
            }
        },
        "AutoDeploymentsEnabled": true
    }' \
    --instance-configuration '{
        "Cpu": "1 vCPU",
        "Memory": "2 GB"
    }' \
    --region $AWS_REGION
```

## Step 5: Get Your Service URL

After deployment (takes 3-5 minutes), get your service URL:

```bash
aws apprunner list-services --region $AWS_REGION
```

Or check the AWS Console: **App Runner → Services**

The URL will look like: `https://xxxxx.us-east-1.awsapprunner.com`

## Step 6: Test the Deployment

### Health Check
```bash
export APP_RUNNER_URL="https://xxxxx.us-east-1.awsapprunner.com"
curl $APP_RUNNER_URL/
```

Expected response:
```json
{"status": "ok"}
```

### Upload Test Audio
```bash
curl -X POST "$APP_RUNNER_URL/upload" \
     -F "file=@/path/to/test-audio.wav" \
     -F "device_id=esp32_prod_01" \
     -F "latitude=37.7749" \
     -F "longitude=-122.4194" \
     -F "zip_code=94102" \
     -F "noise_level=72.3"
```

### Export Data
```bash
curl "$APP_RUNNER_URL/export" -o production-data.csv
```

## Step 7: Configure ESP32 Devices

Update your ESP32 firmware to point to the production URL:

In `embedded/esp32_noise_mapping_edge/src/main.cpp`:

```cpp
const char* host = "xxxxx.us-east-1.awsapprunner.com";  // Your App Runner domain
const int httpPort = 443;  // HTTPS
const char* uploadPath = "/upload";
```

**Important:** You'll need to add HTTPS/TLS support to the ESP32 code for production. See the Arduino WiFiClientSecure library.

## Step 8: Monitor and Maintain

### View Logs
```bash
aws apprunner list-operations \
    --service-arn $(aws apprunner list-services --query 'ServiceSummaryList[0].ServiceArn' --output text) \
    --region $AWS_REGION
```

Or use CloudWatch Logs in the AWS Console.

### Check S3 Storage
```bash
aws s3 ls s3://iot-noise-mapping-audio/
```

### Query DynamoDB
```bash
aws dynamodb scan --table-name NoiseClassification --region $AWS_REGION
```

### Update the Service

After making code changes:

1. Build and push new Docker image (Step 3)
2. App Runner auto-deploys if `AutoDeploymentsEnabled: true`
3. Or manually trigger: `aws apprunner start-deployment --service-arn <arn>`

## Cost Optimization

### App Runner Pricing
- Pay only for CPU and memory used
- Automatically scales to zero when not in use
- Typical cost: $5-20/month for low traffic

### S3 Pricing
- $0.023 per GB/month (Standard storage)
- Consider lifecycle policies to move old audio to Glacier

### DynamoDB Pricing
- Pay-per-request: $1.25 per million writes
- $0.25 per million reads
- Free tier: 25 GB storage

### Reduce Costs
```bash
# S3 lifecycle policy (archive after 90 days)
aws s3api put-bucket-lifecycle-configuration \
    --bucket iot-noise-mapping-audio \
    --lifecycle-configuration file://lifecycle.json

# lifecycle.json
{
  "Rules": [{
    "Id": "Archive old audio",
    "Status": "Enabled",
    "Transitions": [{
      "Days": 90,
      "StorageClass": "GLACIER"
    }]
  }]
}
```

## Security Best Practices

### IAM Roles
- Use IAM roles instead of access keys when possible
- Principle of least privilege
- Rotate credentials regularly

### Network Security
- App Runner runs in AWS-managed VPC (no public IP)
- Enable HTTPS only (no HTTP)
- Consider API Gateway + WAF for additional protection

### Data Security
- Enable S3 bucket encryption
- Enable DynamoDB encryption at rest
- Use VPC endpoints for S3/DynamoDB access

### Enable S3 Encryption
```bash
aws s3api put-bucket-encryption \
    --bucket iot-noise-mapping-audio \
    --server-side-encryption-configuration '{
        "Rules": [{
            "ApplyServerSideEncryptionByDefault": {
                "SSEAlgorithm": "AES256"
            }
        }]
    }'
```

## Troubleshooting

### Deployment Fails
- Check IAM permissions for App Runner service role
- Verify ECR image exists and is accessible
- Check CloudWatch Logs for startup errors

### ESP32 Can't Connect
- Verify App Runner URL is correct
- Check ESP32 has internet access
- Ensure HTTPS/TLS is properly configured (port 443)

### High Costs
- Check App Runner instance count and scaling
- Review S3 storage usage: `aws s3 ls s3://iot-noise-mapping-audio --recursive --summarize`
- Enable cost alerts in AWS Billing

### Classification Errors
- Check TensorFlow model downloads in App Runner logs
- Verify memory allocation (increase to 4 GB if needed)
- Test with known good audio files

## Cleanup / Deletion

To delete all resources and stop charges:

```bash
# Delete App Runner service
aws apprunner delete-service \
    --service-arn $(aws apprunner list-services --query 'ServiceSummaryList[0].ServiceArn' --output text)

# Delete S3 bucket (must be empty first)
aws s3 rm s3://iot-noise-mapping-audio --recursive
aws s3 rb s3://iot-noise-mapping-audio

# Delete DynamoDB table
aws dynamodb delete-table --table-name NoiseClassification

# Delete ECR repository
aws ecr delete-repository \
    --repository-name noise-mapping-service \
    --force
```

## Next Steps

- Set up CloudWatch alarms for monitoring
- Configure auto-scaling policies
- Add API authentication (API Gateway + Cognito)
- Implement data visualization dashboard
- Set up CI/CD pipeline (GitHub Actions → ECR → App Runner)
