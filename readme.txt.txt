-------------------------------------------------------------This Project Is Based on ESP 32-S3 N16R8--------------------------------------------------------------------------------------
DESCRIPTION: The Project is External Hosting which Displays the Real Time CPU, Ram Statistics i.e. usage, battery percentage, SSD, GPU in one Interface working in first OLED Display, whereas the Second OLED Screen Shows Real-Time Spotify Player i.e. Song Name, Artist, Time Span.

HARDWARE:
ESP 32-S3 N16R8 
2X I2C 0.96 OLED SCREEN
//Jumper wires or Zero PCB or Bread Board (as your use case)

HARDWARE USER & MANUAL:
ESP 32-S3 N16R8: https://documentation.espressif.com/esp32-s3_technical_reference_manual_en.pdf
0.96 OLED I2C DISPLAY: https://www.rajguruelectronics.com/Product/1147/OLED%204%20Pin%200.96%20inch%20128x64%20display%20module%20%20yellow%20bule.pdf

HARDWARE CONNECTIONS:
OLED 1: SDA = GPIO 8, SCL/SCK = GPIO 9
OLED 2: SDA = GPIO 17, SCL/SCK = GPIO 18
Both on 3.3V and GND  
   
FIRMWARE SETTINGS ON ARUDINO IDE:
USB CDC On Boot: "Enabled"   CPU Frequency: "240MHz (WiFi)"   Core Debug Level: "None"   USB DFU On Boot: "Disabled"   Erase All Flash Before Sketch Upload: "Disabled"   Events Run On: "Core 1"   Flash Mode: "DIO 80MHz"   Flash Size: "16MB (128Mb)"   JTAG Adapter: "Disabled"   Arduino Runs On: "Core 1"   USB Firmware MSC On Boot: "Disabled"   Partition Scheme: "16M Flash (3MB APP/9.9MB FATFS)"   PSRAM: "Disabled"   Upload Mode: "UART0 / Hardware CDC"   Upload Speed: "921600"   USB Mode: "Hardware CDC and JTAG"\

NOTE: You can use Default Setting but if your ESP getting crashing use:
Flash Mode: "DIO 80MHz"
PSRAM: "Disabled"
//Reason as PSRAM is Manufactured by Third Patry Companies it will not match the desired frequency for flashing code.

SOFTWARE CHANGES:
Need to Install python version less than 3.12 
install PIP

then run the code in the terminal / cmd Powershell in the directory you have the files.

   py -3.12 pc_agent.py --port COM18 

// Note: Port Number Varies according to your Devices Check in Device Manager    