# IoT-Based Noise Pollution Mapping

This project is an **IoT-based noise pollution monitoring system** that:
- Uses **ESP32** sensors with MAX9814 microphones to capture and classify environmental audio
- Stores **audio files on AWS S3** for persistent storage
- Processes data using **FastAPI & TensorFlow (YAMNet/PANNs)** for audio classification
- Saves classification results in **AWS DynamoDB** with geolocation metadata
- Supports both **cloud deployment** (AWS App Runner) and **local development** (LocalStack)

## 📁 Project Structure

```
iot-noise-mapping/
├── backend/                      # FastAPI service for audio processing
│   ├── main.py                   # Main API endpoints and logic
│   ├── classifier.py             # Audio classification module
│   ├── config.py                 # Configuration settings
│   ├── requirements.txt          # Python dependencies
│   ├── Dockerfile                # Container image for deployment
│   ├── db/                       # Database utilities
│   │   ├── db_handler.py         # DynamoDB operations
│   │   └── schema.sql            # Database schema
│   └── uploads/                  # Temporary audio file storage
├── embedded/                     # IoT device firmware
│   └── esp32_noise_mapping_edge/ # ESP32 with MAX9814 microphone
│       ├── platformio.ini        # PlatformIO configuration
│       └── src/
│           └── main.cpp          # ESP32 firmware (audio capture & upload)
├── scripts/                      # Deployment and setup scripts
│   ├── setup-aws.sh              # AWS infrastructure setup
│   └── deploy.sh                 # App Runner deployment
├── docs/                         # Detailed documentation
│   ├── local-development.md      # LocalStack setup guide
│   └── aws-deployment.md         # Production AWS deployment guide
└── README.md                     # This file
```

## 🚀 Quick Start

### Local Development with LocalStack

For local development and testing without AWS costs, see the detailed guide:

**📖 [Local Development Guide](docs/local-development.md)**

Quick start:
```bash
# Start LocalStack
localstack start

# Set environment variables
export LOCAL_AWS_ACCESS_KEY="test"
export LOCAL_AWS_ACCESS_SECRET="test"

# Run the backend
cd backend
python3 main.py
```

### AWS Production Deployment

For production deployment to AWS App Runner with real AWS services:

**📖 [AWS Deployment Guide](docs/aws-deployment.md)**

Quick start:
```bash
# Setup AWS infrastructure
chmod +x scripts/setup-aws.sh
./scripts/setup-aws.sh

# Deploy to App Runner
chmod +x scripts/deploy.sh
./scripts/deploy.sh
```

## 🔧 Prerequisites

- **Python 3.10+** → [Install Python](https://www.python.org/downloads/)
- **Docker** → [Install Docker](https://docs.docker.com/get-docker/)
- **LocalStack** (for local dev) → `pip install localstack`
- **AWS CLI** (for production) → [Install AWS CLI](https://aws.amazon.com/cli/)
- **PlatformIO** (for ESP32) → [Install PlatformIO](https://platformio.org/install)

## 🎯 Key Features

- **Real-time Audio Classification**: YAMNet and PANNs models for sound event detection
- **Edge Computing**: Audio capture and preprocessing on ESP32 devices
- **Scalable Storage**: S3 for audio files, DynamoDB for metadata
- **Geolocation Support**: GPS coordinates and noise level tracking
- **Local Development**: Full LocalStack integration for offline development
- **Serverless Deployment**: AWS App Runner for automatic scaling

## 📊 API Endpoints

- `POST /upload` - Upload and classify audio from IoT devices
- `GET /export` - Export classification data as CSV
- `GET /` - Health check endpoint

## 📄 License

See [LICENSE](LICENSE) file for details.