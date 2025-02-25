import os
import librosa
import numpy as np
import tensorflow as tf
import tensorflow_hub as hub
from fastapi import FastAPI, File, UploadFile
from starlette.responses import JSONResponse

# Initialize FastAPI
app = FastAPI()

# Load YAMNet model
YAMNET_MODEL_HANDLE = "https://tfhub.dev/google/yamnet/1"
yamnet = hub.load(YAMNET_MODEL_HANDLE)

# Load YAMNet class labels
CLASS_MAP = []
with open("yamnet_class_map.csv", "r", encoding="utf-8") as f:
    CLASS_MAP = [line.strip().split(",")[-1] for line in f.readlines()]  # Keep only class names

UPLOAD_FOLDER = "./uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)  # Ensure the upload folder exists

def classify_audio(audio_path):
    """ Classifies an audio file using YAMNet """
    try:
        audio_data, sr = librosa.load(audio_path, sr=16000)
        scores, _, _ = yamnet(audio_data)

        # Ensure scores are not empty
        if scores.numpy().size == 0:
            return "Unknown", 0.0

        mean_scores = np.mean(scores.numpy(), axis=0)
        top_class = np.argmax(mean_scores)

        # Ensure valid class index
        if top_class >= len(CLASS_MAP):
            return "Unknown", 0.0

        confidence = mean_scores[top_class]
        return CLASS_MAP[top_class], float(confidence)
    except Exception as e:
        return f"Error: {str(e)}", 0.0

@app.post("/upload")
async def upload_audio(file: UploadFile = File(...)):
    """ Handles file upload and classification """
    try:
        file_path = os.path.join(UPLOAD_FOLDER, file.filename)
        with open(file_path, "wb") as buffer:
            buffer.write(file.file.read())

        # Classify the uploaded file
        label, confidence = classify_audio(file_path)

        return JSONResponse(content={
            "classification": {"category": label, "confidence": confidence}
        })
    except Exception as e:
        return JSONResponse(content={"error": str(e)}, status_code=500)

# Run the FastAPI server
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)