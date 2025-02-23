import json
import boto3
import librosa
import numpy as np
import tensorflow as tf
import tensorflow_hub as hub

# Load YAMNet model from TensorFlow Hub
YAMNET_MODEL_HANDLE = "https://tfhub.dev/google/yamnet/1"
yamnet = hub.load(YAMNET_MODEL_HANDLE)

# AWS Clients
s3 = boto3.client("s3")

# Class labels for YAMNet
CLASS_MAP = [
    "Silence",
    "Speech",
    "Music",
    "Traffic",
    "Sirens",
    "Animals",
    "Water",
    "Wind",
]


def classify_audio(audio_data, sample_rate):
    """Classifies an audio clip using YAMNet"""
    scores, embeddings, spectrogram = yamnet(audio_data)
    mean_scores = np.mean(scores.numpy(), axis=0)
    top_class = np.argmax(mean_scores)
    confidence = mean_scores[top_class]
    return CLASS_MAP[top_class], float(confidence)


def lambda_handler(event, context):
    """Main Lambda handler function"""
    try:
        # Parse the event for S3 bucket and object key
        s3_bucket = event["s3_bucket"]
        s3_key = event["s3_key"]

        # Download audio file from S3
        tmp_file = "/tmp/audio.wav"
        s3.download_file(s3_bucket, s3_key, tmp_file)

        # Load and process audio
        audio_data, sample_rate = librosa.load(tmp_file, sr=16000)
        label, confidence = classify_audio(audio_data, sample_rate)

        return {
            "statusCode": 200,
            "body": json.dumps({"category": label, "confidence": confidence}),
        }

    except Exception as e:
        return {"statusCode": 500, "error": str(e)}
