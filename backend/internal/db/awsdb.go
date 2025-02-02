package db

import (
	"context"
	"fmt"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/dynamodb"
	"github.com/aws/aws-sdk-go-v2/service/dynamodb/types"
	"github.com/hasankhatib/iot-noise-mapping/internal/models"
)

type AWSDB struct {
	client *dynamodb.Client
	table  string
}

func NewAWSDB(tableName string) (*AWSDB, error) {
	cfg, err := config.LoadDefaultConfig(context.TODO())
	if err != nil {
		return nil, err
	}
	return &AWSDB{
		client: dynamodb.NewFromConfig(cfg),
		table:  tableName,
	}, nil
}

func (db *AWSDB) SaveNoiseData(data models.NoiseData) error {
	_, err := db.client.PutItem(context.TODO(), &dynamodb.PutItemInput{
		TableName: aws.String(db.table),
		Item: map[string]types.AttributeValue{
			"device_id": &types.AttributeValueMemberS{Value: data.DeviceID},
			"timestamp": &types.AttributeValueMemberS{Value: data.Timestamp.Format(time.RFC3339)},
			"location":  &types.AttributeValueMemberS{Value: data.Location},
			"avg_db":    &types.AttributeValueMemberN{Value: fmt.Sprintf("%f", data.AvgDB)},
			"max_db":    &types.AttributeValueMemberN{Value: fmt.Sprintf("%f", data.MaxDB)},
			"battery":   &types.AttributeValueMemberN{Value: fmt.Sprintf("%f", data.Battery)},
		},
	})
	return err
}
