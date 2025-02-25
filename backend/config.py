import os

# AWS Configuration
AWS_REGION = "eu-north-1"
S3_BUCKET = "iot-noise-mapping-audio"
DYNAMODB_TABLE = "NoiseClassification"

# Load from environment variables if needed
AWS_ACCESS_KEY = os.getenv("LOCAL_AWS_ACCESS_KEY", "_")
AWS_SECRET_KEY = os.getenv("LOCAL_AWS_ACCESS_SECRET", "_")