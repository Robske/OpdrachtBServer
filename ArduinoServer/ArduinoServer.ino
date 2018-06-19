// Include files.
#include <dht.h>                  // humidity / temperature sensor
#include <SPI.h>                  // Ethernet shield uses SPI-interface
#include <Ethernet.h>             // Ethernet library (use Ethernet2.h for new ethernet shield v2)

// Set Ethernet Shield MAC address  (check yours)
byte mac[] = { 0x40, 0x6c, 0x8f, 0x36, 0x84, 0x8a }; // Ethernet adapter shield
int ethPort = 3300;                                  // Take a free port (check your router)
EthernetServer server(ethPort);              // EthernetServer instance (listening on port <ethPort>).
bool connected = false;                  // Used for retrying DHCP

#define ledpin 8                         // Led shows if client connected

dht DHT;
#define DHT11_PIN 7

int temp, humidity;                      // Variables for dht sensor 
int airTime = 3600;                      // each airTime in seconds put fans on for 1 min

void setup()
{  
   Serial.begin(9600);
   //while (!Serial) { ; }               // Wait for serial port to connect. Needed for Leonardo only.
   
   pinMode(ledpin, OUTPUT);
   digitalWrite(ledpin, LOW);
  
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
      delay(1000);
      return; // wait for connection
   }

   Serial.println("Application connected");
   digitalWrite(ledpin, HIGH);
   // Do what needs to be done while the socket is connected.
   while (ethernetClient.connected())
   {
      // Check if actions needed
      DoActionsNeeded();
      
      //sensorValue = readSensor(0, 100);         // update sensor value
   
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

// Implementation of (simple) protocol between app and Arduino
// Request (from app) is single char ('a', 's', 't', 'i' etc.)
// Response (to app) is 4 chars  (not all commands demand a response)
void executeCommand(char cmd)
{     
         char buf[4] = {'\0', '\0', '\0', '\0'};

         // Command protocol
         Serial.print("App send '"); Serial.print(cmd); Serial.print("] -> ");
         
         switch (cmd) {
          // Case a is example of value (int) to buf
         /*case 'a': // Report sensor value to the app  
            intToCharBuf(sensorValue, buf, 4);                // convert to charbuffer 
            server.write(buf, 4);                             // response is always 4 chars (\n included)
            Serial.print("Sensor: "); Serial.println(buf);
            break;*/
         case 's':
            //if (doorOpen) { server.write("Open"); Serial.println("Open"); }
            //else { server.write("Clos"); Serial.println("Closed"); }
            break;
         default:
            break;
         }
}

// Calls multiple functions and acts on values
void DoActionsNeeded() {
  temp = getTemp();
  //if (
}

// Get temperature
int getTemp()
{
  int chk = DHT.read11(DHT11_PIN);
  
   //  Get value from sensor
   int a = DHT.temperature;

   Serial.print(a);
   String b = String(a);
   a = b.substring(0, b.indexOf(',')).toInt();
   Serial.print(a);
   return a;
   //return map(analogRead(pn), 0, 1023, 0, mx-1);    
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

