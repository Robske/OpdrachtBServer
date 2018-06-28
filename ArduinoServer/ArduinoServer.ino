bool debug = true;

// Include files.
#include <dht.h>                  // humidity / temperature sensor
#include <SPI.h>                  // Ethernet shield uses SPI-interface
#include <Ethernet.h>             // Ethernet library (use Ethernet2.h for new ethernet shield v2)

// Set Ethernet Shield MAC address  (check yours)
byte mac[] = { 0x40, 0x6c, 0x8f, 0x36, 0x84, 0x8a }; // Ethernet adapter shield
int ethPort = 3300;                                  // Take a free port (check your router)
EthernetServer server(ethPort);                      // EthernetServer instance (listening on port <ethPort>).
bool connected = false;                              // Used for retrying DHCP

dht DHT;

#define LDRpin A0     // Pin Light Sensor
#define SOILpin A1    // Pin Soil Humidity Sensor
#define FAN1pin 2     // Pin Fans
#define FAN2pin 3     // Pin Fans
#define PUMPpin 5     // Pin Waterpump
#define LEDpin 6      // Led shows if client connected
#define DHT11pin 7    // Pin Air Humidity and Temperature Sensor
#define LIGHTpin 8    // Pin Light

bool fansOn = false;
bool pumpOn = false;
bool lightOn = false;

bool pumpManualSet = false;   //If true the pump doesn't rely on soilhumidity, instead just listens to the app
bool fanManualSet = false;    //If true the fans don't rely on timer, instead just listens to the app
bool lightManualSet = false;    //If true the light don't rely on lightmeter, instead just listens to the app

int temp, airhumidity, soilhumidity, light = 0;      // Variables for dht sensor 
int airTime = 3600;                                  // each airTime in seconds put fans on for 1 min

unsigned long updateValuesPreviousMillis = 0;                 // Cares for a count
const long updateValuesInterval = 5000;                      // interval how long fans must be on (milliseconds)

unsigned long fansPreviousMillis = 0;                 // Cares for a count
const long fansInterval = 10000;                      // interval how long fans must be on (milliseconds)

unsigned long pumpManualSetPreviousMillis = 0;                 // Cares for a count
const long fansOnEach = 30000;                        // interval at which to turn fans on (milliseconds)
const long pumpManualSetInterval = 30000;                      // interval how long fans must be on (milliseconds)

void setup()
{  
   Serial.begin(9600);
   
   pinMode(LEDpin, OUTPUT);
   digitalWrite(LEDpin, LOW);
   pinMode(FAN1pin, OUTPUT);
   digitalWrite(FAN1pin, LOW);
   pinMode(FAN2pin, OUTPUT);
   digitalWrite(FAN2pin, LOW);
   pinMode(PUMPpin, OUTPUT);
   digitalWrite(PUMPpin, LOW);
   pinMode(LIGHTpin, OUTPUT);
   digitalWrite(LIGHTpin, LOW);
  
   Serial.println("Server started, trying to get IP...");

   //Try to get an IP address from the DHCP server.
   if (Ethernet.begin(mac) == 0)
   {
      Serial.println("Could not obtain IP-address from DHCP");
      Serial.println("Retry in 10 seconds");
      delay(10000);
      
      for (int i = 0; i < 10; i++) {
        if (Ethernet.begin(mac) != 0) {
          connected = true;
          return;
        } else {
          Serial.print("Try "); Serial.print(i); Serial.println(" failed");
          Serial.println("Retry in 10 seconds");
          delay(10000);
        }
      }

      if (!connected) {
        Serial.println("Could not get ip from DHCP, stopped retrying");
        
        while (true){     // no point in carrying on, so do nothing forevermore; check your router
          
        }
      }
   }
   
   Serial.println("Ethernetboard connected (pins 10, 11, 12, 13 and SPI)");
   Serial.println("Connect to DHCP source in local network (blinking led -> waiting for connection)");
   
   //Start the ethernet server.
   server.begin();

   // Print IP-address and led indication of server state
   Serial.print("Listening address: ");
   Serial.print(Ethernet.localIP());
   
   // for hardware debug: LED indication of server state: blinking = waiting for connection
   int IPnr = getIPComputerNumber(Ethernet.localIP());   // Get computernumber in local network 192.168.1.3 -> 3)
   Serial.print(" ["); Serial.print(IPnr); Serial.print("] "); 
   Serial.print("  [Testcase: telnet "); Serial.print(Ethernet.localIP()); Serial.print(" "); Serial.print(ethPort); Serial.println("]");
}

void loop()
{
   // Listen for incomming connection (app)
   EthernetClient ethernetClient = server.available();

   if (!ethernetClient) {
      DoActionsNeeded();
      return; // wait for connection
   }

   Serial.println("Application connected");
   digitalWrite(LEDpin, HIGH);
   
   // Do what needs to be done while the socket is connected.
   while (ethernetClient.connected())
   {
      // Check if actions needed
      DoActionsNeeded();
   
      // Execute when byte is received.
      while (ethernetClient.available())
      {
        // Read client character
         char inByte = ethernetClient.read();   // Get byte from the client.
         executeCommand(inByte);                // Wait for command to execute
         inByte = NULL;                         // Reset the read byte.
      } 
   }
   
   digitalWrite(LEDpin, LOW);
   changeLightState(false);
   changeFanState(false);
   changePumpState(false);

   lightManualSet = false;
   pumpManualSet = false;
   fanManualSet = false;
   Serial.println("Application disonnected");
}

// Calls multiple functions and acts on values
void DoActionsNeeded() {  
  unsigned long currentMillis = millis();

  if (currentMillis - updateValuesPreviousMillis >= updateValuesInterval) {
    Serial.println("Updating all sensor values");
    updateValuesPreviousMillis = currentMillis;
    updateTempAndHumidity();
    soilhumidity = getSoilHumidity(100);
    light = getLight(100);

    Serial.println("New Values");
    Serial.print("Light = ");
    Serial.print(light);
    Serial.print(" ON: ");
    Serial.println(lightOn);
    Serial.print("Air humidity = ");
    Serial.print(airhumidity);
    Serial.print(" ON: ");
    Serial.println(fansOn);
    Serial.print("Ground humidity = ");
    Serial.print(soilhumidity);
    Serial.print(" ON: ");
    Serial.println(pumpOn);
    Serial.print("Temp = ");
    Serial.println(temp);
  }  
  
  if (!pumpManualSet && soilhumidity < 30 && !pumpOn) { // Check if soilhumidity is to low
    changePumpState(true);
  } else if (!pumpManualSet && pumpOn && soilhumidity >= 50) {
    changePumpState(false);
  }

  if (!fanManualSet && !fansOn && currentMillis - fansPreviousMillis >= fansOnEach) {
    fansPreviousMillis = currentMillis;
    changeFanState(true);
  } else if (!fanManualSet && fansOn && currentMillis - fansPreviousMillis >= fansInterval) {
    fansPreviousMillis = currentMillis;
    changeFanState(false);
  }
  
  if (!lightManualSet && light < 30 && !lightOn) { // Check if light is needed
    changeLightState(true);
  } else if (!lightManualSet && light > 50 && lightOn) {
    changeLightState(false);
  }
}

// Implementation of (simple) protocol between app and Arduino
// Request (from app) is single char ('a', 's', 't', 'i' etc.)
// Response (to app) is 4 chars  (not all commands demand a response)
void executeCommand(char cmd)
{     
  char buf[4] = {'\0', '\0', '\0', '\0'};
  
  // Command protocol
  Serial.print("App send ["); Serial.print(cmd); Serial.println("] -> ");
  
  switch (cmd) {
    case 'h':
      intToCharBuf(soilhumidity, buf, 4);
      server.write(buf, 4);
      Serial.print("Send soil humidity: "); Serial.println(buf);
      break;
    case 'a':
      intToCharBuf(airhumidity, buf, 4);
      server.write(buf, 4);
      Serial.print("Send air humidity: "); Serial.println(buf);
      break;
    case 't':
      intToCharBuf(temp, buf, 4);
      server.write(buf, 4);
      Serial.print("Send temp: "); Serial.println(buf);
      break;
    case 'l':
      intToCharBuf(light, buf, 4);
      server.write(buf, 4);
      Serial.print("Send light: "); Serial.println(buf);
      break;
    case 'R':
      pumpManualSet = true;
      changePumpState(true);
      break;
    case 'r':
      pumpManualSet = false;
      changePumpState(false);
      break;
    case 'W':
      fanManualSet = true;
      changeFanState(true);      
      break;
    case 'w':
      fanManualSet = false;
      changeFanState(false);
      fansPreviousMillis = millis();
      break;
    case 'Z':
    lightManualSet = true;
      changeLightState(true);
      break;
    case 'z':
      lightManualSet = false;
      changeLightState(false);
      break;
    default:
      break;
    }
}

// Get temperature
int updateTempAndHumidity()
{
  int chk = DHT.read11(DHT11pin);
  temp = DHT.temperature;
  airhumidity = DHT.humidity;
}

int getSoilHumidity(int maxval) { //FUNCTION STILL HAS TO BE MADE
  return map(analogRead(SOILpin), 0, 1023, 0, maxval);
}

int getLight(int maxval) {  
  return map(analogRead(LDRpin), 0, 1023, 0, maxval);
}

void changeLightState(bool on) {
  if (on) {
    digitalWrite(LIGHTpin, HIGH);
    lightOn = true;
  } else {
    digitalWrite(LIGHTpin, LOW);
    lightOn = false;
  }
}

void changeFanState(bool on) {
  if (on) {
    digitalWrite(FAN1pin, HIGH);
    digitalWrite(FAN2pin, HIGH);
    fansOn = true;
  } else {
    digitalWrite(FAN1pin, LOW);
    digitalWrite(FAN2pin, LOW);
    fansOn = false;
  }

  Serial.print("Fan = ");
  Serial.println(fansOn);
  Serial.print("Fans manual set = ");
  Serial.println(fanManualSet);
}

void changePumpState(bool on) {
  if (on) {
    digitalWrite(PUMPpin, HIGH);
    pumpOn = true;
  } else {
    digitalWrite(PUMPpin, LOW);
    pumpOn = false;
  }
}

// Convert int <val> char buffer with length <len>
void intToCharBuf(int val, char buf[], int len)
{
   String s;
   s = String(val);                        // convert tot string
   if (s.length() == 1) s = "0" + s;       // prefix redundant "0"
   if (s.length() == 2) s = "0" + s;       // prefix redundant "0" if needed again
   s = s + "\n";                           // add newline
   s.toCharArray(buf, len);                // convert string to char-buffer
}

// Convert IPAddress tot String (e.g. "192.168.1.105")
String IPAddressToString(IPAddress address)
{
    return String(address[0]) + "." + 
           String(address[1]) + "." + 
           String(address[2]) + "." + 
           String(address[3]);
}

// Returns B-class network-id: 192.168.1.3 -> 1)
int getIPClassB(IPAddress address)
{
    return address[2];
}

// Returns computernumber in local network: 192.168.1.3 -> 3)
int getIPComputerNumber(IPAddress address)
{
    return address[3];
}

// Returns computernumber in local network: 192.168.1.105 -> 5)
int getIPComputerNumberOffset(IPAddress address, int offset)
{
    return getIPComputerNumber(address) - offset;
}
