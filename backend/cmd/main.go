package main

import (
	"log"
	"net/http"
	"os"

	"github.com/hasankhatib/iot-noise-mapping/internal/config"
	"github.com/hasankhatib/iot-noise-mapping/internal/db"
	"github.com/hasankhatib/iot-noise-mapping/internal/handlers"
)

func main() {
	// Load environment variables
	config.LoadEnv()

	var database db.Database
	var err error

	// Choose database based on environment variable
	if os.Getenv("USE_AWS_DB") == "true" {
		database, err = db.NewAWSDB("NoiseDataTable")
		if err != nil {
			log.Fatalf("Failed to connect to AWS DynamoDB: %v", err)
		}
	} else {
		database, err = db.NewLocalDB("noise_data.db") // SQLite for local testing
		if err != nil {
			log.Fatalf("Failed to connect to local database: %v", err)
		}
	}

	// Initialize the handler with the selected database
	handlers.InitDB(database)

	// Define HTTP routes
	http.HandleFunc("/api/noise-data", handlers.HandleNoiseData)

	log.Println("Server running on port 8080...")
	log.Fatal(http.ListenAndServe(":8080", nil))
}
