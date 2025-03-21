import os
import boto3
import librosa
import numpy as np
import tensorflow as tf
import csv
import tensorflow_hub as hub
from decimal import Decimal
from fastapi import FastAPI, File, UploadFile
from fastapi import Form
from starlette.responses import JSONResponse, StreamingResponse
from config import AWS_REGION, S3_BUCKET, DYNAMODB_TABLE, AWS_ACCESS_KEY, AWS_SECRET_KEY
import uuid
from datetime import datetime, timezone

# Initialize FastAPI
app = FastAPI()

# Log AWS access key and secret
print(f"AWS Access Key: {AWS_ACCESS_KEY}")
print(f"AWS Secret Key: {AWS_SECRET_KEY}")

# Initialize AWS Clients with local credentials
session = boto3.Session(
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY,
    region_name=AWS_REGION,
)

s3_client = session.client("s3")
dynamodb = session.resource("dynamodb")
table = dynamodb.Table(DYNAMODB_TABLE)

# Disable GPU usage for TensorFlow
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

# Log TensorFlow device information
print("TensorFlow is running on the following devices:")
for device in tf.config.list_physical_devices():
    print(f"- {device.device_type}: {device.name}")

# Load YAMNet model
YAMNET_MODEL_HANDLE = "https://tfhub.dev/google/yamnet/1"
yamnet = hub.load(YAMNET_MODEL_HANDLE)

# Load Class Labels
CLASS_MAP = []

with open("yamnet_class_map.csv", "r", encoding="utf-8") as f:
    reader = csv.reader(f)
    next(reader)  # Skip header
    CLASS_MAP = [row[2].strip() for row in reader]  # Ensure proper class mapping

print(f"Loaded {len(CLASS_MAP)} class labels.")  # Debugging step

UPLOAD_FOLDER = "./uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)


def classify_audio(audio_path):
    """Classifies an audio file using YAMNet"""
    try:
        # Load and preprocess audio
        audio_data, sr = librosa.load(audio_path, sr=16000, mono=True)
        audio_data = np.array(audio_data, dtype=np.float32)  # Ensure correct dtype
        audio_data = np.squeeze(audio_data)  # Ensure it's a 1D array

        # Run YAMNet model
        scores, embeddings, spectrogram = yamnet(audio_data)

        # Ensure YAMNet returns results
        if scores.shape[0] == 0:
            return "Unknown", 0.0

        # Compute mean scores & get top category
        mean_scores = np.mean(scores.numpy(), axis=0)
        top_class = np.argmax(mean_scores)

        print(f"Top Class Index: {top_class}, Confidence: {mean_scores[top_class]}")

        # Ensure class index is within range
        if top_class >= len(CLASS_MAP):
            return "Unknown", 0.0

        confidence = mean_scores[top_class]
        
        print("Top Class Index:", top_class, "Class Label:", CLASS_MAP[top_class], "Confidence:", confidence)

        return CLASS_MAP[top_class], float(confidence)

    except Exception as e:
        return f"Error: {str(e)}", 0.0


@app.post("/upload")
async def upload_audio(
    file: UploadFile = File(...),
    device_id: str = Form("default_device"),
    longitude: str = Form(None),
    latitude: str = Form(None),
    zip_code: str = Form(None),
    noise_level: float = Form(None)  # New parameter
):
    """ Uploads WAV file to S3 and stores classification metadata in DynamoDB """
    try:
        # Generate timestamp and formatted time
        timestamp = int(datetime.now(timezone.utc).timestamp())
        formatted_time = datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
        filename = f"{device_id}_{formatted_time}.wav"  # Use underscores

        # Save file locally
        file_path = os.path.join(UPLOAD_FOLDER, filename)
        with open(file_path, "wb") as buffer:
            buffer.write(file.file.read())

        # Upload to S3
        s3_client.upload_file(file_path, S3_BUCKET, filename)

        # Classify audio
        label, confidence = classify_audio(file_path)

        # Generate UUID for the record
        record_uuid = str(uuid.uuid4())

        # Store metadata in DynamoDB
        table.put_item(Item={
            "UUID": record_uuid,  # Hash key
            "filename": filename,
            "time": formatted_time,
            "timestamp": timestamp,
            "device_id": device_id,
            "zipcode": zip_code,
            "longitude": longitude,
            "latitude": latitude,
            "noise_level": Decimal(str(noise_level)) if noise_level else None,  # Save noise_level
            "label": label,
            "confidence": Decimal(str(confidence)) if confidence else None
        })

        return JSONResponse(content={
            "classification": {"category": label, "confidence": confidence},
            "metadata": {
                "UUID": record_uuid,
                "filename": filename,
                "time": formatted_time,
                "timestamp": timestamp,
                "device_id": device_id,
                "zipcode": zip_code,
                "longitude": longitude,
                "latitude": latitude,
                "noise_level": noise_level  # Include noise_level in response
            }
        })
    except Exception as e:
        return JSONResponse(content={"error": str(e)}, status_code=500)

@app.get("/export")
def export_data():
    """Exports stored classification results as a CSV file"""
    response = table.scan()
    items = response["Items"]

    csv_data = "filename,timestamp,location,label,confidence\n"
    for item in items:
        csv_data += f"{item['filename']},{item['timestamp']},{item['location']},{item['label']},{item['confidence']}\n"

    return StreamingResponse(
        iter([csv_data]),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=classified_data.csv"},
    )

@app.get("/")
def read_root():
    return {"status": "ok"}

if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8080)
