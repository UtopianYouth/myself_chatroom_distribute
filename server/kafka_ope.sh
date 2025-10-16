#!/bin/bash

# Define base directory for Kafka
BASE_DIR="/home/utopianyouth/environment/kafka_2.13-3.7.2"

# Check if Kafka directory exists
if [ ! -d "$BASE_DIR" ]; then
    echo "Kafka directory not found at $BASE_DIR"
    exit 1
fi

# Define script and config paths
ZOOKEEPER_START_SCRIPT="$BASE_DIR/bin/zookeeper-server-start.sh"
KAFKA_START_SCRIPT="$BASE_DIR/bin/kafka-server-start.sh"
ZOOKEEPER_STOP_SCRIPT="$BASE_DIR/bin/zookeeper-server-stop.sh"
KAFKA_STOP_SCRIPT="$BASE_DIR/bin/kafka-server-stop.sh"

ZOOKEEPER_CONFIG="$BASE_DIR/config/zookeeper.properties"
KAFKA_CONFIG="$BASE_DIR/config/server.properties"

# Function to check for required files
check_files() {
    for file in "$ZOOKEEPER_START_SCRIPT" "$KAFKA_START_SCRIPT" "$ZOOKEEPER_CONFIG" "$KAFKA_CONFIG" "$ZOOKEEPER_STOP_SCRIPT" "$KAFKA_STOP_SCRIPT"; do
        if [ ! -f "$file" ]; then
            echo "Error: Required file not found - $file"
            exit 1
        fi
    done
}

# Function to start services
start() {
    echo "Starting Zookeeper..."
    "$ZOOKEEPER_START_SCRIPT" -daemon "$ZOOKEEPER_CONFIG"
    if [ $? -ne 0 ]; then
        echo "Failed to start Zookeeper."
        exit 1
    fi
    echo "Zookeeper started."

    echo "Waiting for Zookeeper to initialize..."
    sleep 5

    echo "Starting Kafka..."
    "$KAFKA_START_SCRIPT" -daemon "$KAFKA_CONFIG"
    if [ $? -ne 0 ]; then
        echo "Failed to start Kafka. Stopping Zookeeper..."
        "$ZOOKEEPER_STOP_SCRIPT"
        exit 1
    fi
    echo "Kafka started."
    echo "Infrastructure (Zookeeper, Kafka) started successfully."
}

# Function to stop services
stop() {
    echo "Stopping Kafka..."
    "$KAFKA_STOP_SCRIPT"
    if [ $? -ne 0 ]; then
        echo "Kafka may not have stopped gracefully."
    else
        echo "Kafka stopped."
    fi

    sleep 2

    echo "Stopping Zookeeper..."
    "$ZOOKEEPER_STOP_SCRIPT"
    if [ $? -ne 0 ]; then
        echo "Zookeeper may not have stopped gracefully."
    else
        echo "Zookeeper stopped."
    fi
    
    echo "Infrastructure stopped."
}

# Main script logic
check_files

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        sleep 3
        start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
esac

exit 0