const WebSocket = require('ws');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const express = require('express');
const http = require('http');
const cors = require('cors');
const fs = require('fs');
const path = require('path');

class SmartHomeServer {
    constructor() {
        this.port = 8080;
        this.serialPortPath = 'COM7'; // Windows COM port
        this.baudRate = 9600;
        
        // Data storage
        this.sensorData = {};
        this.deviceStates = {};
        this.clients = new Set();
        this.arduinoConnected = false;
        this.lastDataReceived = null;
        
        // Initialize server components
        this.initializeExpressServer();
        this.initializeWebSocketServer();
        this.initializeArduinoConnection();
        this.setupHeartbeat();
        
        console.log('🏠 Enhanced Smart Home Server Starting (Windows)...');
    }

    initializeExpressServer() {
        this.app = express();
        this.server = http.createServer(this.app);
        
        // Middleware
        this.app.use(cors());
        this.app.use(express.json());
        this.app.use(express.static('public')); // Serve static files
        
        // Health check endpoint
        this.app.get('/health', (req, res) => {
            res.json({
                status: 'healthy',
                arduinoConnected: this.arduinoConnected,
                connectedClients: this.clients.size,
                lastDataReceived: this.lastDataReceived,
                uptime: process.uptime(),
                platform: 'Windows',
                serialPort: this.serialPortPath
            });
        });
        
        // API endpoints
        this.app.get('/api/status', (req, res) => {
            res.json({
                sensorData: this.sensorData,
                deviceStates: this.deviceStates,
                arduinoConnected: this.arduinoConnected
            });
        });
        
        this.app.post('/api/command', (req, res) => {
            const { command } = req.body;
            if (command) {
                this.sendCommandToArduino(command);
                res.json({ success: true, command });
            } else {
                res.status(400).json({ error: 'Command required' });
            }
        });
        
        // Get historical data (placeholder for future database integration)
        this.app.get('/api/history', (req, res) => {
            res.json({
                message: 'Historical data endpoint - integrate with database',
                currentData: this.sensorData
            });
        });
    }

    initializeWebSocketServer() {
        this.wss = new WebSocket.Server({ 
            server: this.server,
            path: '/'
        });
        
        this.wss.on('connection', (ws, req) => {
            console.log(`🔗 New WebSocket connection from ${req.socket.remoteAddress}`);
            this.clients.add(ws);
            
            // Send current data to new client
            if (Object.keys(this.sensorData).length > 0) {
                ws.send(JSON.stringify(this.sensorData));
            }
            
            // Handle incoming messages from clients
            ws.on('message', (message) => {
                try {
                    const data = JSON.parse(message);
                    this.handleClientMessage(data, ws);
                } catch (error) {
                    console.error('❌ Error parsing client message:', error);
                    ws.send(JSON.stringify({ error: 'Invalid JSON format' }));
                }
            });
            
            // Handle client disconnect
            ws.on('close', () => {
                console.log('🔌 WebSocket client disconnected');
                this.clients.delete(ws);
            });
            
            // Handle connection errors
            ws.on('error', (error) => {
                console.error('❌ WebSocket error:', error);
                this.clients.delete(ws);
            });
        });
    }

    initializeArduinoConnection() {
        this.connectToArduino();
    }

    async connectToArduino() {
        try {
            // List available ports for debugging
            const ports = await SerialPort.list();
            console.log('📋 Available serial ports on Windows:');
            ports.forEach(port => {
                console.log(`   ${port.path} - ${port.manufacturer || 'Unknown'} - ${port.serialNumber || 'N/A'}`);
            });
            
            // Check if COM7 is available
            const targetPort = ports.find(port => port.path === this.serialPortPath);
            if (!targetPort) {
                console.warn(`⚠️  ${this.serialPortPath} not found in available ports`);
            }
            
            // Try to connect to Arduino
            this.serialPort = new SerialPort({
                path: this.serialPortPath,
                baudRate: this.baudRate,
                autoOpen: false
            });
            
            this.parser = this.serialPort.pipe(new ReadlineParser({ delimiter: '\n' }));
            
            // Handle successful connection
            this.serialPort.on('open', () => {
                console.log(`✅ Connected to Arduino on ${this.serialPortPath}`);
                this.arduinoConnected = true;
                this.broadcastConnectionStatus();
                
                // Send initial status request after Arduino setup time
                setTimeout(() => {
                    this.sendCommandToArduino('STATUS');
                }, 3000); // Increased delay for Arduino setup
            });
            
            // Handle incoming data from Arduino
            this.parser.on('data', (data) => {
                this.handleArduinoData(data.trim());
            });
            
            // Handle connection errors
            this.serialPort.on('error', (error) => {
                console.error('❌ Arduino connection error:', error.message);
                this.arduinoConnected = false;
                this.broadcastConnectionStatus();
                
                // Attempt to reconnect after delay
                setTimeout(() => {
                    this.reconnectToArduino();
                }, 5000);
            });
            
            // Handle port closure
            this.serialPort.on('close', () => {
                console.log('🔌 Arduino connection closed');
                this.arduinoConnected = false;
                this.broadcastConnectionStatus();
                
                // Attempt to reconnect after delay
                setTimeout(() => {
                    this.reconnectToArduino();
                }, 3000);
            });
            
            // Open the connection
            this.serialPort.open();
            
        } catch (error) {
            console.error('❌ Failed to initialize Arduino connection:', error.message);
            
            // Try alternative Windows COM ports
            const alternativePorts = ['COM3', 'COM4', 'COM5', 'COM6', 'COM8', 'COM9', 'COM10'];
            this.tryAlternativePorts(alternativePorts);
        }
    }

    async tryAlternativePorts(ports) {
        console.log('🔄 Trying alternative Windows COM ports...');
        
        for (const portPath of ports) {
            try {
                console.log(`🔄 Trying alternative port: ${portPath}`);
                
                // Check if port exists
                const availablePorts = await SerialPort.list();
                const portExists = availablePorts.find(p => p.path === portPath);
                
                if (portExists) {
                    this.serialPortPath = portPath;
                    await this.connectToArduino();
                    break;
                } else {
                    console.log(`❌ Port ${portPath} not available`);
                }
            } catch (error) {
                console.log(`❌ Failed to connect to ${portPath}: ${error.message}`);
            }
        }
    }

    reconnectToArduino() {
        if (!this.arduinoConnected) {
            console.log('🔄 Attempting to reconnect to Arduino...');
            this.connectToArduino();
        }
    }

    handleArduinoData(data) {
        try {
            // Parse JSON data from Arduino
            const parsedData = JSON.parse(data);
            
            // Update sensor data
            this.sensorData = { ...this.sensorData, ...parsedData };
            this.lastDataReceived = new Date().toISOString();
            
            // Update device states for easier access
            this.updateDeviceStates(parsedData);
            
            // Broadcast to all connected WebSocket clients
            this.broadcastToClients(parsedData);
            
            // Log important events
            this.logImportantEvents(parsedData);
            
        } catch (error) {
            // Handle non-JSON messages (like status messages)
            if (data.includes('status') || data.includes('error') || data.includes('Ready')) {
                console.log(`📊 Arduino: ${data}`);
                this.broadcastToClients({ message: data });
            } else {
                console.error('❌ Error parsing Arduino data:', error.message);
                console.log('Raw data:', data);
            }
        }
    }

    updateDeviceStates(data) {
        // Extract device states for easy reference
        const deviceMapping = {
            lights: {
                livingRoom: data.livingRoom,
                bedroom: data.bedroom,
                kitchen: data.kitchen,
                bathroom: data.bathroom
            },
            leds: {
                A2: data.ledA2,
                A3: data.ledA3,
                A4: data.ledA4,
                A5: data.ledA5
            },
            climate: {
                fan: data.fan,
                ac: data.ac
            },
            automation: {
                autoLight: data.autoLight,
                autoTemp: data.autoTemp
            },
            security: {
                alarm: data.alarm,
                motion: data.motion,
                tempAlert: data.tempAlert
            }
        };
        
        this.deviceStates = deviceMapping;
    }

    logImportantEvents(data) {
        const timestamp = new Date().toLocaleString();
        
        // Log temperature alerts
        if (data.tempAlert && !this.sensorData.tempAlert) {
            console.log(`🌡️  [${timestamp}] Temperature Alert Activated - ${data.temperature}°C`);
        }
        
        // Log security events
        if (data.alarm && !this.sensorData.alarm) {
            console.log(`🚨 [${timestamp}] Security Alarm Triggered - Gas: ${data.gas}`);
        }
        
        // Log motion detection
        if (data.motion && !this.sensorData.motion) {
            console.log(`👤 [${timestamp}] Motion Detected`);
        }
        
        // Log system errors
        if (data.systemError) {
            console.log(`⚠️  [${timestamp}] System Error - DHT Errors: ${data.dhtErrors}`);
        }
    }

    handleClientMessage(data, ws) {
        console.log('📨 Received client message:', data);
        
        if (data.command) {
            // Forward command to Arduino
            this.sendCommandToArduino(data.command);
            
            // Send acknowledgment back to client
            ws.send(JSON.stringify({
                type: 'command_ack',
                command: data.command,
                timestamp: new Date().toISOString()
            }));
        } else if (data.type === 'ping') {
            // Respond to ping with pong
            ws.send(JSON.stringify({
                type: 'pong',
                timestamp: new Date().toISOString()
            }));
        } else if (data.type === 'request_status') {
            // Send current status
            ws.send(JSON.stringify({
                type: 'status',
                data: this.sensorData,
                arduinoConnected: this.arduinoConnected
            }));
        }
    }

    sendCommandToArduino(command) {
        if (this.arduinoConnected && this.serialPort && this.serialPort.isOpen) {
            try {
                this.serialPort.write(command + '\n');
                console.log(`📤 Sent command to Arduino: ${command}`);
            } catch (error) {
                console.error('❌ Error sending command to Arduino:', error.message);
            }
        } else {
            console.warn('⚠️  Cannot send command - Arduino not connected');
            
            // Notify clients about connection issue
            this.broadcastToClients({
                error: 'Arduino not connected',
                command: command
            });
        }
    }

    broadcastToClients(data) {
        const message = JSON.stringify(data);
        
        this.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN) {
                try {
                    client.send(message);
                } catch (error) {
                    console.error('❌ Error sending data to client:', error.message);
                    this.clients.delete(client);
                }
            } else {
                // Remove closed connections
                this.clients.delete(client);
            }
        });
    }

    broadcastConnectionStatus() {
        this.broadcastToClients({
            type: 'connection_status',
            arduinoConnected: this.arduinoConnected,
            timestamp: new Date().toISOString(),
            serialPort: this.serialPortPath
        });
    }

    setupHeartbeat() {
        // Send heartbeat every 30 seconds
        setInterval(() => {
            if (this.arduinoConnected) {
                this.sendCommandToArduino('STATUS');
            }
            
            // Clean up disconnected clients
            this.clients.forEach(client => {
                if (client.readyState !== WebSocket.OPEN) {
                    this.clients.delete(client);
                }
            });
            
            console.log(`💓 Heartbeat - Arduino: ${this.arduinoConnected ? '✅' : '❌'} (${this.serialPortPath}), Clients: ${this.clients.size}`);
        }, 30000);
    }

    // Data logging functionality (for future database integration)
    logSensorData(data) {
        const logEntry = {
            timestamp: new Date().toISOString(),
            ...data
        };
        
        // For now, just log to console (can be extended to database)
        if (data.temperature && data.humidity) {
            console.log(`📊 Sensor Log: ${logEntry.timestamp} - T:${data.temperature}°C, H:${data.humidity}%`);
        }
    }

    // Windows-specific port discovery
    async discoverArduinoPorts() {
        try {
            const ports = await SerialPort.list();
            const arduinoPorts = ports.filter(port => 
                port.manufacturer && (
                    port.manufacturer.toLowerCase().includes('arduino') ||
                    port.manufacturer.toLowerCase().includes('ch340') ||
                    port.manufacturer.toLowerCase().includes('cp210') ||
                    port.manufacturer.toLowerCase().includes('ftdi')
                )
            );
            
            console.log('🔍 Detected Arduino-compatible ports:');
            arduinoPorts.forEach(port => {
                console.log(`   ${port.path} - ${port.manufacturer} - ${port.serialNumber || 'N/A'}`);
            });
            
            return arduinoPorts;
        } catch (error) {
            console.error('❌ Error discovering ports:', error);
            return [];
        }
    }

    // Graceful shutdown
    shutdown() {
        console.log('🔄 Shutting down Smart Home Server...');
        
        // Close Arduino connection
        if (this.serialPort && this.serialPort.isOpen) {
            this.serialPort.close();
        }
        
        // Close WebSocket connections
        this.clients.forEach(client => {
            client.close();
        });
        
        // Close HTTP server
        this.server.close(() => {
            console.log('✅ Server shutdown complete');
            process.exit(0);
        });
    }

    start() {
        this.server.listen(this.port, () => {
            console.log(`🚀 Smart Home Server running on port ${this.port}`);
            console.log(`🌐 WebSocket server ready for connections`);
            console.log(`📡 Connecting to Arduino on ${this.serialPortPath}...`);
            console.log(`🖥️  Platform: Windows`);
            console.log(`📊 Dashboard: http://localhost:${this.port}`);
        });
    }
}

// Initialize and start the server
const smartHomeServer = new SmartHomeServer();
smartHomeServer.start();

// Handle graceful shutdown on Windows
process.on('SIGINT', () => {
    console.log('\n🛑 Received SIGINT signal (Ctrl+C)');
    smartHomeServer.shutdown();
});

process.on('SIGTERM', () => {
    console.log('\n🛑 Received SIGTERM signal');
    smartHomeServer.shutdown();
});

// Windows-specific shutdown handling
process.on('SIGBREAK', () => {
    console.log('\n🛑 Received SIGBREAK signal');
    smartHomeServer.shutdown();
});

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
    console.error('❌ Uncaught Exception:', error);
    smartHomeServer.shutdown();
});

process.on('unhandledRejection', (reason, promise) => {
    console.error('❌ Unhandled Rejection at:', promise, 'reason:', reason);
});

module.exports = SmartHomeServer;