package db

import (
	"github.com/hasankhatib/iot-noise-mapping/internal/models"
)

type Database interface {
	SaveNoiseData(data models.NoiseData) error
}
