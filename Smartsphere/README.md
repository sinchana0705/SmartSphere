SmartSphere

SmartSphere is an enhanced smart home management system integrating Arduino-based sensors and actuators with a modern web dashboard for real-time monitoring and control. The system uses temperature, gas, ultrasonic, light, and motion sensors alongside LEDs, buzzers, and relays to automate and secure your home environment.

Features

- Real-time sensor data acquisition: temperature, humidity, gas levels, light intensity, motion detection
- Control of room lighting, LEDs, fan, and AC via serial commands and web dashboard
- Automated lighting based on motion and ambient light conditions
- Temperature-based climate control with fan and AC activation
- Gas leak detection with buzzer alarm and visual alerts
- Persistent user settings saved in EEPROM on the Arduino
- Responsive frontend dashboard built with HTML, CSS, and Chart.js
- Modular, extensible architecture for customization and expansion

 Setup

 Hardware-

- Arduino board connected to sensors:
  - DHT11 (Temperature & Humidity) on pin 4
  - Gas sensor on analog pin A0
  - LDR light sensor on analog pin A1
  - PIR motion sensor on pin 2
  - LEDs on pins A2–A5 and for each room’s lighting
  - Buzzer on pin 8, relays on pins 6 and 7 for AC and fan

 Software-

1. Clone the repository:  

2. Install dependencies: 
(If you have backend Node.js server)  

3. Upload Arduino code: 
Open `sketch_jul13/sketch_jul13.ino` in Arduino IDE and upload to your board.

4. Configure backend:  
Make sure backend server matches Arduino serial port (default COM7 or adjust accordingly).

5. Run backend server:

6. Open frontend dashboard: 
Open `public/index.html` in your browser.

 Usage

Monitor real-time sensor data and control devices through the dashboard interface. Use toggle switches for lighting and appliances. The system will alert you visually and with buzzer alarms under dangerous conditions (e.g., gas leaks, temperature alerts).

 License

This project is licensed under the MIT License.

---

Feel free to expand or customize this README with more detailed instructions, screenshots, or contribution guidelines as needed.

