# CloudDrop-SeniorProject

## Project Name: CloudDrop

**Executive Summary**
CloudDrop is an innovative IoT solution designed to monitor, detect, and log the occurrence of specific sound events, such as those produced by a hand sanitizer dispenser.  The system leverages sound detection technology to identify events, timestamps and logs them, then periodically logs them to a cloud platform.  By providing real-time analytics and insights, CloudDrop aims to enhance hygiene compliance monitoring in public and private spaces.  The solution is lightweight, cost-effective, and scalable, with intended usage in a wide range of industries such as healthcare, hospitality, retail, and education.

**Project Objectives**
•	Usage Detection
Develop a system capable of identifying specific sound/vibration patterns associated with hand sanitizer usage.  Detection should be done via a low-power pickup device, like a piezo-electric contact.

•	Real-Time Logging
Implement functionality to timestamp and log detected events.  Logging to continue independently of reporting connection availability 

•	WiFi connection
Internet connectivity through WiFi enabled to periodically sync time with NTP servers and upload event storage logs (like thru MQTT)

•	Low-power for Battery Operation:
Designed to operate for extended periods of time on a replaceable coin-cell.

•	Contact mountable
Housing enclosure that can be mounted against a surface via two-sided sticky tape.

**Technical Requirements**
1. Hardware Requirements
•	Microcontroller:   ESP32-C3
Debug and initial implementation via Seeed Studio XIAO ESP32C3
•	ADC 
- Microcontroller/Processor:
- Sound Detection Module:
- Placeholder for microphone and processing capabilities.
- Power Supply:
- Placeholder for battery capacity or wired options.
- Connectivity:
- Placeholder for Wi-Fi or other communication modules.
2. Software Requirements
•	WiFi only
•	Base F/W on ESP32-C3:    Tasmota
•	Cloud Service Periodic Connection:
o	NTP
o	MQTT
o	Push notifications for Critical Events (Battery,etc.)
•	
- Firmware:
- Placeholder for sound processing and event detection algorithms.
- Cloud Platform:
- Placeholder for integration specifics (e.g., AWS, Google Cloud, Azure, or custom backend).
- Data Logging:
- Placeholder for database type (e.g., Firebase, PostgreSQL, etc.).
- User Interface:
- Placeholder for dashboard or mobile app requirements.
3. Network Requirements
- Placeholder for bandwidth and connectivity standards.
4. Security Requirements
- Placeholder for encryption standards and data protection measures.
5. Testing and Validation
  - Placeholder for testing protocols (e.g., sound recognition accuracy, network resilience, etc.).
Key Milestones and Deliverables
1. Prototype Development:
   - Sound detection module configured and tested.
2. Cloud Integration:
   - Placeholder for backend configuration and API development.
4. Pilot Testing:
   - Placeholder for test environment and use case validation.

**Potential Applications**
- Hygiene compliance monitoring in healthcare facilities.
- Usage tracking in public restrooms and offices.
- Data-driven insights for facility management.
  
**Conclusion**
CloudDrop represents a step forward in leveraging IoT technologies for promoting hygiene and safety. By seamlessly integrating sound detection with cloud logging, the system delivers actionable insights to enhance operational efficiency and compliance monitoring. The project holds significant potential to drive better hygiene practices across various industries as well as determining consumable use, placement, and life-cycle.
