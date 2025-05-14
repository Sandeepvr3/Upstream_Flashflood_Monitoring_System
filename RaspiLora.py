import torch
import torch.nn as nn
import numpy as np
import pandas as pd
import threading
import spidev
import RPi.GPIO as GPIO
from flask import Flask, jsonify, render_template
from time import sleep
from LoRa import LoRa 

# Flask App
server = Flask(__name__)

# ------------------------- Load Trained SNN Model -------------------------

class DummyFloodModel(nn.Module):
    def __init__(self):
        super(DummyFloodModel, self).__init__()
        self.fc1 = nn.Linear(4, 10)
        self.fc2 = nn.Linear(10, 1)

    def forward(self, x):
        x = torch.relu(self.fc1(x))
        x = torch.sigmoid(self.fc2(x))
        return x

model = DummyFloodModel()
model.load_state_dict(torch.load("snn_model.pth"))
model.eval()

def predict_danger_level(data):
    inputs = torch.tensor([data], dtype=torch.float32)
    risk_score = model(inputs).item()
    print(risk_score, " - risk score")
    if risk_score < 0.7:
        return "SAFE"
    elif risk_score < 2:
        return "WARNING"
    else:
        return "DANGER"

# ------------------------- Flask API Routes -------------------------

@server.route("/data")
def get_data():
    """Fetch latest sensor data and predict danger level"""
    try:
        with open("sensor_data.txt", "r") as file:
            lines = file.readlines()
            if not lines:
                print("File is empty")
                return jsonify({
                    "rainfall": 0, "flow_rate": 0, "water_level": 0, "wind_speed": 0, "danger_level": "SAFE"
                })
            last_line = [line.strip() for line in lines if line.strip()][-1]
            print(f"Last line in file: {last_line}")

        latest_data = [float(x) for x in last_line.split(",")]
        if len(latest_data) != 4:
            print(f"Invalid data format: {latest_data}")
            return jsonify({"error": "Invalid data format"}), 500

        danger_level = predict_danger_level(latest_data)
        print(f"Processed data: {latest_data}, Danger: {danger_level}")

        return jsonify({
            "rainfall": latest_data[0],
            "flow_rate": latest_data[1],
            "water_level": latest_data[2],
            "wind_speed": latest_data[3],
            "danger_level": danger_level
        })

    except Exception as e:
        print(f"Error fetching data: {e}")
        return jsonify({"error": "Could not fetch data"}), 500

@server.route("/")
def dashboard():
    """Serves the dashboard UI"""
    return render_template("dashboard.html")

# ------------------------- LoRa SX1278 Data Receiver -------------------------

# LoRa SX1278 Configuration
LORA_CS = 24  # Chip Select (NSS)
LORA_RESET = 25  # Reset Pin
LORA_DIO0 = 24  # Interrupt Pin

GPIO.setmode(GPIO.BCM)
GPIO.setup(LORA_CS, GPIO.OUT, initial=GPIO.HIGH)
GPIO.setup(LORA_RESET, GPIO.OUT, initial=GPIO.HIGH)
GPIO.setup(LORA_DIO0, GPIO.IN)

# Initialize LoRa Module
lora = LoRa(spi_bus=0, spi_device=0, frequency=433E6, tx_power=14)

def receive_lora_data():
    """Receives sensor data over LoRa SX1278 from ESP32"""
    try:
        print("Listening for LoRa messages...")
        with open("sensor_data.txt", "a", buffering=1) as file:
            while True:
                if lora.received():
                    data = lora.receive().decode('utf-8').strip()
                    file.write(data + "\n")
                    file.flush()
                    print(f"Received and written: {data}")
                sleep(1)
    except Exception as e:
        print(f"LoRa Error: {e}")

# Run LoRa listener in a separate thread
threading.Thread(target=receive_lora_data, daemon=True).start()

# ------------------------- Run Flask Server -------------------------

if __name__ == "__main__":
    server.run(host="0.0.0.0", port=5000, debug=True)
