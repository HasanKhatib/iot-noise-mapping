# IoT-Based Noise Pollution Mapping

This project is an **IoT-based noise pollution monitoring system** that:
- Uses **Arduino Nano RP2040** & **ESP32** sensors to collect noise levels.
- Stores **audio files on AWS S3**.
- Processes data using **FastAPI & TensorFlow (YAMNet)**.
- Saves classification results in **AWS DynamoDB**.
- Runs on **AWS App Runner (Serverless, No EC2 Required)**.


## Repository Structure
- `backend` - Contains the backend code for the project.
- `edge-devices/**` - MicroPython/C++ firmware for IoT sensors (Arduino Nano RP2040 & ESP32).

- `frontend` - Contains the frontend code for the project.
    - `aws-dashboards` - AWS-based dashboards for data visualization.
- `data` - Contains collected datasets for noise pollution analysis.
- `docs` - Contains research and documentation related to the project.

## 🔧 Prerequisites
Before setting up the project, install:  
- 🐍 **Python 3.10+** → [Install Python](https://www.python.org/downloads/)  
- 🐳 **Docker** → [Install Docker](https://docs.docker.com/get-docker/)  
- ☁️ **AWS CLI** → [Install AWS CLI](https://aws.amazon.com/cli/)


## ⚡ Setup AWS Infrastructure (Run Once)
Run the setup script to **automate AWS resource creation**:  
```sh
chmod +x scripts/setup-aws.sh
./scripts/setup-aws.sh
```
✅ This will:
- Create an S3 bucket (iot-noise-mapping-audio)
- Create a DynamoDB table (NoiseClassification)
- Set up an IAM user & policies
- Prepare AWS Elastic Container Registry (ECR)

## 🏗 Build & Deploy the FastAPI Service
- Retrieve AWS Account ID and set it as an environment variable:
```sh
export AWS_ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)
```
- Build & Push Docker Image to AWS ECR
```sh
aws ecr get-login-password --region eu-west-1 | docker login --username AWS --password-stdin $AWS_ACCOUNT_ID.dkr.ecr.eu-west-1.amazonaws.com

docker build -t noise-mapping-service .

docker tag noise-mapping-service:latest $AWS_ACCOUNT_ID.dkr.ecr.eu-west-1.amazonaws.com/noise-mapping-service:latest

docker push $AWS_ACCOUNT_ID.dkr.ecr.eu-west-1.amazonaws.com/noise-mapping-service:latest
```

- Deploy the Service to AWS App Runner
```sh
chmod +x scripts/deploy.sh
./scripts/deploy.sh
```

## 🚀 Test the service

- Upload an audio file
```sh
curl -X POST "https://<APP_RUNNER_URL>/upload" \
     -H "Content-Type: multipart/form-data" \
     -F "file=@./ID_0009P22A.wav" \
     -F "device_id=RP2040_01"
```

- 📊 Export Classification Data as CSV
```sh
curl -X GET "https://<APP_RUNNER_URL>/export"
```