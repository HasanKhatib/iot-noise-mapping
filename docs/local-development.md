# Local Development with LocalStack

This guide covers setting up the IoT Noise Mapping system for local development using LocalStack, which emulates AWS services on your machine.

## Prerequisites

- Python 3.10+
- Docker Desktop running
- LocalStack installed: `pip install localstack`

## Step 1: Start LocalStack

LocalStack provides local AWS service emulation (S3, DynamoDB, etc.)

```bash
localstack start
```

This will start LocalStack on `http://localhost:4566` with all AWS services available locally.

## Step 2: Set Environment Variables

Configure the backend to use LocalStack:

```bash
export LOCAL_AWS_ACCESS_KEY="test"
export LOCAL_AWS_ACCESS_SECRET="test"
export AWS_REGION="us-east-1"
```

These credentials are dummy values that LocalStack accepts for local development.

## Step 3: Initialize Local AWS Resources

Create the required S3 bucket and DynamoDB table in LocalStack:

```bash
# Create S3 bucket
aws --endpoint-url=http://localhost:4566 s3 mb s3://iot-noise-mapping-audio

# Create DynamoDB table
aws --endpoint-url=http://localhost:4566 dynamodb create-table \
    --table-name NoiseClassification \
    --attribute-definitions \
        AttributeName=UUID,AttributeType=S \
    --key-schema \
        AttributeName=UUID,KeyType=HASH \
    --billing-mode PAY_PER_REQUEST
```

## Step 4: Configure Backend

Update `backend/config.py` to use LocalStack endpoints if not already configured:

```python
# For local development
S3_BUCKET = "iot-noise-mapping-audio"
DYNAMODB_TABLE = "NoiseClassification"
AWS_REGION = "us-east-1"
AWS_ACCESS_KEY = os.getenv("LOCAL_AWS_ACCESS_KEY", "test")
AWS_SECRET_KEY = os.getenv("LOCAL_AWS_ACCESS_SECRET", "test")
```

The backend already uses local endpoints when these environment variables are set:
- S3: `http://localhost:4566`
- DynamoDB: `http://localhost:4566`

## Step 5: Install Python Dependencies

```bash
cd backend
pip install -r requirements.txt
```

## Step 6: Run the Backend

```bash
python3 main.py
```

The API will be available at `http://localhost:8080`

## Step 7: Test the API

### Health Check
```bash
curl http://localhost:8080/
```

Expected response:
```json
{"status": "ok"}
```

### Upload Audio File

```bash
curl -X POST "http://localhost:8080/upload" \
     -F "file=@/path/to/audio.wav" \
     -F "device_id=esp32_test" \
     -F "latitude=37.7749" \
     -F "longitude=-122.4194" \
     -F "zip_code=94102" \
     -F "noise_level=65.5"
```

### Export Data

```bash
curl http://localhost:8080/export -o data.csv
```

## Step 8: Verify Data in LocalStack

### Check S3 files
```bash
aws --endpoint-url=http://localhost:4566 s3 ls s3://iot-noise-mapping-audio/
```

### Check DynamoDB records
```bash
aws --endpoint-url=http://localhost:4566 dynamodb scan \
    --table-name NoiseClassification
```

## Testing with ESP32

Configure your ESP32 to point to your local machine instead of AWS:

In `embedded/esp32_noise_mapping_edge/src/main.cpp`, update the host:

```cpp
const char* host = "YOUR_LOCAL_IP";  // e.g., "192.168.1.100"
const int httpPort = 8080;
```

**Note:** Use your machine's local IP address (not `localhost` or `127.0.0.1`) so the ESP32 can reach it on your network.

## Troubleshooting

### LocalStack not starting
- Ensure Docker Desktop is running
- Check port 4566 is not in use: `lsof -i :4566`
- Try: `localstack stop && localstack start`

### Backend can't connect to LocalStack
- Verify LocalStack is running: `curl http://localhost:4566/_localstack/health`
- Check environment variables are set correctly
- Ensure endpoint URLs in `main.py` point to `http://localhost:4566`

### Audio classification errors
- Ensure the model files are downloaded (happens automatically on first run)
- Check TensorFlow is installed: `pip install tensorflow tensorflow-hub`
- For faster CPU testing, TensorFlow will use CPU by default (see `main.py` line with `CUDA_VISIBLE_DEVICES`)

## Model Selection

The backend supports two audio classification models:

### YAMNet (Default)
```bash
export MODEL_TYPE="yamnet"
```
- 521 sound classes
- Lighter weight
- Better for general audio

### PANNs
```bash
export MODEL_TYPE="panns"
pip install panns-inference
```
- More accurate for complex audio
- Requires additional installation

## Development Workflow

1. **Make changes** to `backend/main.py` or other files
2. **Restart the server**: `Ctrl+C` then `python3 main.py`
3. **Test immediately** - no deployment needed!
4. **Check logs** - all output is visible in the terminal
5. **Inspect data** - use AWS CLI with `--endpoint-url=http://localhost:4566`

## Stopping LocalStack

When you're done developing:

```bash
localstack stop
```

This will stop all LocalStack services but preserve your data. To completely reset:

```bash
localstack stop
docker volume rm localstack-volume
```
