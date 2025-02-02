package models

import "time"

type NoiseData struct {
	DeviceID  string    `json:"device_id"`
	Timestamp time.Time `json:"timestamp"`
	Location  string    `json:"location"`
	AvgDB     float64   `json:"avg_db"`
	MaxDB     float64   `json:"max_db"`
	Battery   float64   `json:"battery"`
}
