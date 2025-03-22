-- Add schema for DynamoDB table
CREATE TABLE RecordingClassification (
    UUID STRING HASH KEY, -- Primary key
    filename STRING,
    time STRING,
    timestamp NUMBER,
    device_id STRING, -- Add device_id field
    zipcode STRING,
    longitude STRING,
    latitude STRING,
    noise_level NUMBER, -- Add noise_level field
    label STRING,
    confidence NUMBER
);
