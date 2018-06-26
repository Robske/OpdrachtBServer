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

#define ledpin 8      // Led shows if client connected

dht DHT;
#define DHT11pin 7    // Pin Air Humidity and Temperature Sensor
#define LDRpin A0     // Pin Light Sensor
#define SOILpin A1    // Pin Soil Humidity Sensor
#define FANSpin 2     // Pin Fans
#define PUMPpin 3     // Pin Waterpump
bool fansOn = false;
bool pumpOn = false;
bool lightOn = false;

bool pumpManualSet = false;   //If true the pump doesn't rely on soilhumidity, instead just listens to the app
bool fanManualSet = false;    //If true the fans don't rely on timer, instead just listens to the app
bool lightManualSet = false;    //If true the light don't rely on lightmeter, instead just listens to the app

int temp, airhumidity, soilhumidity, light = 0;      // Variables for dht sensor 
int airTime = 3600;                                  // each airTime in seconds put fans on for 1 min

unsigned long updateValuesPreviousMillis = 0;                 // Cares for a count
const long updateValuesInterval = 2500;                      // interval how long fans must be on (milliseconds)

unsigned long fansPreviousMillis = 0;                 // Cares for a count
const long fansInterval = 10000;                      // interval how long fans must be on (milliseconds)

unsigned long fansOnPreviousInterval = 0;             // Cares for a count
const long fansOnEach = 30000;                        // interval at which to turn fans on (milliseconds)

void setup()
{  
   Serial.begin(9600);
   
   pinMode(ledpin, OUTPUT);
   digitalWrite(ledpin, LOW);
   pinMode(FANSpin, OUTPUT);
   digitalWrite(FANSpin, LOW);
   pinMode(PUMPpin, OUTPUT);
   digitalWrite(PUMPpin, LOW);
  
   Serial.println("Server started, trying to get IP...");

   //Try to get an IP address from the DHCP server.
   if (Ethernet.begin(mac) == 0)
   {
      Serial.println("Could not obtain IP-address from DHCP");
      Serial.println("Retry in 10 seconds");
      delay(10000);
      
      for (int i = 0; i < 100; i++) {
        if (Ethernet.begin(mac) != 0) {
          connected = true;
          return;
        } else {
          Serial.print("Try"); Serial.print(i); Serial.println(" failed");
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
   digitalWrite(ledpin, HIGH);
   
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
   
   digitalWrite(ledpin, LOW);
   Serial.println("Application disonnected");
}

// Calls multiple functions and acts on values
void DoActionsNeeded() {  
  unsigned long currentMillis = millis();

  if (currentMillis - updateValuesPreviousMillis >= updateValuesInterval) {
    updateValuesPreviousMillis = currentMillis;
    updateTempAndHumidity();
  }

  soilhumidity = getSoilHumidity(100);
  light = getLight(100);

  Serial.println("New Line");
  Serial.println(light);
  Serial.println(airhumidity);
  Serial.println(soilhumidity);
  Serial.println(temp);
  Serial.println("");
  
  if (!pumpManualSet && soilhumidity < 30 && !pumpOn) { // Check if soilhumidity is to low
    digitalWrite(PUMPpin, HIGH);
    pumpOn = true;
    Serial.println("Pump is on");
    Serial.println(soilhumidity);
  } else if (!pumpManualSet && pumpOn && soilhumidity >= 50) {
    digitalWrite(PUMPpin, LOW);
    pumpOn = false;
    Serial.println("Pump is off");
    Serial.println(soilhumidity);
  }

  if (!fanManualSet && !fansOn && currentMillis - fansOnPreviousInterval >= fansOnEach) {
    fansOnPreviousInterval = currentMillis;
    fansPreviousMillis = currentMillis;
    changeFanState(true);
  } else if (!fanManualSet && fansOn && currentMillis - fansPreviousMillis >= fansInterval) {
    fansOnPreviousInterval = currentMillis;
    fansPreviousMillis = currentMillis;
    changeFanState(false);
  }
  
  if (!lightManualSet && light < 30 && !light) { // Check if light is needed
    changeLightState(true);
  } else if (!lightManualSet && light > 50 && light) {
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
    case 'R':
      changePumpState(true);
      pumpManualSet = true;
      break;
    case 'r':
      changePumpState(false);
      pumpManualSet = false;
      break;
    case 'W':
      changeFanState(true);
      fanManualSet = true;
      break;
    case 'w':
      changeFanState(false);
      fanManualSet = false;
      fansPreviousMillis = millis();
      break;
  default:
    break;
       }
}

// Get temperature
int updateTempAndHumidity()
{
  int chk = DHT.read11(DHT11pin);
  
   //  Get value from sensor
   int a = DHT.temperature;
   String astring = String(a);
   temp = astring.substring(0, astring.indexOf(',')).toInt();

   int b = DHT.humidity;
   String bstring = String(b);
   airhumidity = bstring.substring(0, bstring.indexOf(',')).toInt();
}

int getSoilHumidity(int maxval) { //FUNCTION STILL HAS TO BE MADE
  return map(analogRead(SOILpin), 0, 1023, 0, maxval);
}

int getLight(int maxval) {  
  return map(analogRead(LDRpin), 0, 1023, 0, maxval);
}

void changeLightState(bool on) {
  if (on) {
    //KaKu AAN
    lightOn = true;
  } else {
    //KaKu UIT
    lightOn = false;
  }
}

void changeFanState(bool on) {
  if (on) {
    digitalWrite(FANSpin, HIGH);
    fansOn = true;
  } else {
    digitalWrite(FANSpin, LOW);
    fansOn = false;
  }
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






#include <NewRemoteTransmitter.h>

NewRemoteTransmitter transmitter(4255908178, 2, 260, 3);

//bool light1, light2, light3 = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int i = 1; i <= 3; i++) {
    Serial.print("Unit "); Serial.print(i); Serial.println(" is nu aan");
    transmitter.sendUnit(i, true);
    delay(1000);
    
    // Set unit off
    transmitter.sendUnit(i, false);
  }
}

