package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/dynamodb"
	"github.com/aws/aws-sdk-go-v2/service/lambda"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	"github.com/gin-gonic/gin"
)

var s3Client *s3.Client
var lambdaClient *lambda.Client
var dynamoClient *dynamodb.Client

const (
	bucketName = "noise-mapping-audio"
	tableName  = "NoiseClassification"
	lambdaName = "classify_noise"
	region     = "eu-north-1"
)

// Initialize AWS clients
func initAWS() {
	cfg, err := config.LoadDefaultConfig(context.TODO(), config.WithRegion(region))
	if err != nil {
		log.Fatalf("Unable to load AWS config: %v", err)
	}
	s3Client = s3.NewFromConfig(cfg)
	lambdaClient = lambda.NewFromConfig(cfg)
	dynamoClient = dynamodb.NewFromConfig(cfg)
}

// Upload WAV file to S3
func uploadToS3(filePath, deviceID string) (string, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	key := fmt.Sprintf("%s/%s-%d.wav", deviceID, filepath.Base(filePath), time.Now().Unix())

	_, err = s3Client.PutObject(context.TODO(), &s3.PutObjectInput{
		Bucket: aws.String(bucketName),
		Key:    aws.String(key),
		Body:   file,
	})
	if err != nil {
		return "", err
	}

	return key, nil
}

// Call AWS Lambda to classify audio
func classifyNoise(s3Key string) (map[string]interface{}, error) {
	payload, _ := json.Marshal(map[string]string{"s3_key": s3Key})

	resp, err := lambdaClient.Invoke(context.TODO(), &lambda.InvokeInput{
		FunctionName: aws.String(lambdaName),
		Payload:      payload,
	})
	if err != nil {
		return nil, err
	}

	var result map[string]interface{}
	json.Unmarshal(resp.Payload, &result)

	return result, nil
}

func handleUpload(c *gin.Context) {
	file, err := c.FormFile("file")
	if err != nil {
		log.Printf("[ERROR] File upload error: %v", err)
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid file"})
		return
	}

	// Save locally for debugging
	filePath := fmt.Sprintf("./uploads/%s", file.Filename)
	if err := c.SaveUploadedFile(file, filePath); err != nil {
		log.Printf("[ERROR] Failed to save file locally: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save file"})
		return
	}

	// Upload to S3
	s3Key, err := uploadToS3(filePath, "test-device")
	if err != nil {
		log.Printf("[ERROR] Failed to upload to S3: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to upload to S3"})
		return
	}

	// Classify using Lambda
	classification, err := classifyNoise(s3Key)
	if err != nil {
		log.Printf("[ERROR] Lambda classification failed: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to classify audio"})
		return
	}

	log.Printf("[SUCCESS] Classification result: %v", classification)
	c.JSON(http.StatusOK, gin.H{"classification": classification})
}

// Main function
func main() {
	initAWS()
	r := gin.Default()

	// Enable debug logs
	gin.SetMode(gin.DebugMode)
	r.Use(gin.Logger())

	// Add middleware to catch errors
	r.Use(func(c *gin.Context) {
		c.Next()
		for _, err := range c.Errors {
			log.Printf("[ERROR] %v", err.Err)
		}
	})

	r.POST("/upload", handleUpload)
	r.Run(":8080")
}
