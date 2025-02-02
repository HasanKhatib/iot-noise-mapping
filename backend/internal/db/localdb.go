package db

import (
	"database/sql"

	"github.com/hasankhatib/iot-noise-mapping/internal/models"
	_ "github.com/mattn/go-sqlite3"
)

type LocalDB struct {
	db *sql.DB
}

func NewLocalDB(dbPath string) (*LocalDB, error) {
	db, err := sql.Open("sqlite3", dbPath)
	if err != nil {
		return nil, err
	}
	return &LocalDB{db: db}, nil
}

func (db *LocalDB) SaveNoiseData(data models.NoiseData) error {
	_, err := db.db.Exec(`INSERT INTO noise_data (device_id, timestamp, location, avg_db, max_db, battery)
                          VALUES (?, ?, ?, ?, ?, ?)`,
		data.DeviceID, data.Timestamp, data.Location, data.AvgDB, data.MaxDB, data.Battery)
	return err
}
