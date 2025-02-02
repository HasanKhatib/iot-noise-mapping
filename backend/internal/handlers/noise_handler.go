package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/hasankhatib/iot-noise-mapping/internal/db"
	"github.com/hasankhatib/iot-noise-mapping/internal/models"
)

// Global database instance (AWS DynamoDB or SQLite)
var database db.Database

// Initialize the database (called in main)
func InitDB(selectedDB db.Database) {
	database = selectedDB
}

// HandleNoiseData processes the incoming IoT noise data
func HandleNoiseData(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Invalid request method", http.StatusMethodNotAllowed)
		return
	}

	var noiseData models.NoiseData
	decoder := json.NewDecoder(r.Body)
	err := decoder.Decode(&noiseData)
	if err != nil {
		http.Error(w, "Invalid JSON request", http.StatusBadRequest)
		return
	}

	// Ensure timestamp is properly formatted
	noiseData.Timestamp = time.Now()

	// Save data to the selected database
	err = database.SaveNoiseData(noiseData)
	if err != nil {
		http.Error(w, fmt.Sprintf("Failed to save data: %v", err), http.StatusInternalServerError)
		return
	}

	// Send success response
	w.WriteHeader(http.StatusCreated)
	json.NewEncoder(w).Encode(map[string]string{
		"message": "Noise data saved successfully",
	})
}
