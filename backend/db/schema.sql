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
    label STRING,
    confidence NUMBER
);
