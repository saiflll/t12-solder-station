package main

import (
	"net/http"
	"sync"

	"github.com/gin-gonic/gin"
)

type Telemetry struct {
	Ambient float64 `json:"ambient"`
	Tip     float64 `json:"tip"`
	Voltage float64 `json:"voltage"`
	PWM     int     `json:"pwm"`
	Power   float64 `json:"power"`
	Status  string  `json:"status"`
}

var (
	currData Telemetry
	mu       sync.Mutex
)

func main() {
	r := gin.Default()

	r.Static("/view", "./view")

	r.GET("/", func(c *gin.Context) {
		c.File("./view/index.html")
	})

	r.POST("/post", func(c *gin.Context) {
		var json Telemetry
		if err := c.ShouldBindJSON(&json); err == nil {
			mu.Lock()
			currData = json
			mu.Unlock()
			c.JSON(http.StatusOK, gin.H{"status": "ok"})
		} else {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		}
	})

	r.GET("/api/data", func(c *gin.Context) {
		mu.Lock()
		defer mu.Unlock()
		c.JSON(http.StatusOK, currData)
	})

	r.Run(":3000")
}
