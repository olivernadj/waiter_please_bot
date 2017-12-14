#include <SPI.h>
#include <Ethernet.h>
#include <Servo.h>
#include <IRremote.h>

#define rc_btn_0       0xFDB04F
#define rc_btn_1       0xFD00FF
#define rc_btn_2       0xFD807F
#define rc_btn_3       0xFD40BF
#define rc_btn_4       0xFD20DF
#define rc_btn_left    0xFD28D7
#define rc_btn_right   0xFD6897
#define rc_btn_power   0xFDA857
#define rc_repeat      0xFFFFFFFF

// dump http content instead of evaluate. only for debug
const bool printResponseOnly = false;

const int servoCtrlPin    = 7; // servo controller pin
const int irRecvCtrlPin   = 6; // IR receiver receiver pin
const int servoLedPin     = 5; // servo moving indicator LED
const int ethernetLedPin  = 3; // ethernet working indicator LED
const int errorLedPin     = 2; // error indicator LED
const int initDeg = 120; // resting degree
int curDeg = initDeg; //current degree

// last time you connected to the server, in milliseconds
// delay between updates, in milliseconds
// the "L" is needed to use long type numbers
unsigned long lastConnectionTime = 0;
unsigned long lastServoShaftTime = 0;
const unsigned long postingInterval = 5L * 1000L;

// the full target url
char url[] = "http://httpbin.org:80/get?q=arduino&anyslap=1";
char * url_host;
int url_port;
char * url_path;

Servo myservo; //create servo object to control a servo
IRrecv irrecv(irRecvCtrlPin); // create instance of 'irrecv'
decode_results results;      // create instance of 'decode_results'

// have to assign a MAC address for your controller below
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
// initialize the Ethernet client library
EthernetClient client;


// connect or reconnect to the Ethernet network
void ethernetConnect() {
  digitalWrite(ethernetLedPin, HIGH);
  Serial.println("Start the Ethernet connection...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to configure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
    digitalWrite(errorLedPin, HIGH);
  } else {
    digitalWrite(errorLedPin, LOW);
  }
  digitalWrite(ethernetLedPin, LOW);
  // print the Ethernet board/shield's IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
}


void parseUrl() {
  char * pch;
  int i = 0;
  pch = strtok (url,":/");
  while (pch != NULL) {
    switch (++i) {
      case 2:
        url_host = pch;
        break;
      case 3:
        url_port = atoi(pch);
        break;
      case 4:
        url_path = pch;
        break;
    }
    pch = strtok (NULL,":/");
  }
}


// this method makes a HTTP connection to the server:
void httpRequest() {
  digitalWrite(ethernetLedPin, HIGH);
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();

  // print the Ethernet board/shield's IP address:
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  // if there's a successful connection:
  if (client.connect(url_host, url_port)) {
    Serial.println("connecting...");
    // send the HTTP GET request:
    client.print("GET /");
    client.print(url_path);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(url_host);
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();
    digitalWrite(errorLedPin, LOW);
  } else {
    // if you couldn't make a connection:
    digitalWrite(errorLedPin, HIGH);
    Serial.println("connection failed");
  }
  // note the time that the connection was made:
  lastConnectionTime = millis();
  digitalWrite(ethernetLedPin, LOW);
}


int getHttpSlaps() {
  int slaps = 0;

  // if ten seconds have passed since last http request,
  // then do the request again and send data:
  if (millis() - lastConnectionTime > postingInterval) {
    httpRequest();
  }

  // if the client is available then read the stream
  if (client.available()) {
    if (printResponseOnly) {
      char contentBuffer[16];
      int readBytes;
      int i;
      while (client.available()) {
        readBytes = client.readBytes(contentBuffer, 15);
        for (i = 0; i < readBytes; i = i + 1) {
          Serial.write(contentBuffer[i]);
        }
      }
    } else {
      if (client.find((char*)"anyslap", 7)) {
        Serial.println("[http content found]");
        Serial.println("\treading client...");
        Serial.print("\t");
        while (client.available()) {
          char c = client.read();
          Serial.write(c);
          if (isDigit(c)) {
            slaps = c - '0';
            Serial.print("\n\tdigit found:");
            Serial.println(c);
            Serial.print("\tslap:");
            Serial.println(slaps);
            break;
          }
        }
      } else {
        Serial.println("[http content unmatched]");
      }
      Serial.print("slaps:");
      Serial.println(slaps);
      client.stop();
    }
  }
  return slaps;
}


unsigned long getIRCommand() {
  static unsigned long lastCommand = 0; // last IR command
  unsigned long currentCommand = 0; // current IR command
  if (irrecv.decode(&results)) { // have we received an IR signal?
    if (results.value != rc_repeat) {//repeat
      currentCommand = results.value;
      lastCommand = results.value;
    } else {
      Serial.println("repeat..");
      currentCommand = lastCommand;
    }
    irrecv.resume(); // receive the next value
    Serial.println(currentCommand, HEX);
  } else {
    currentCommand = 0;
  }
  return currentCommand;
}


void servoShaft(int angle) {
  if (!myservo.attached()) {
    digitalWrite(servoLedPin, HIGH);
    Serial.println("Attach to servo...");
    myservo.attach(servoCtrlPin);
    myservo.write(curDeg);
  }
  myservo.write(angle);
  // note the time that the connection was made:
  lastServoShaftTime = millis();
}


void doSlap(int slaps) {
  Serial.print("slap:");
  Serial.println(slaps);
  delay(50);
  servoShaft(curDeg - 90);
  delay(300);
  for(int i=1; i<slaps; i++){
    servoShaft(curDeg - 30);
    delay(200);
    servoShaft(curDeg - 90);
    delay(200);
  }
  servoShaft(curDeg);
  delay(300);
}


void setup() {
  // set up LEDs. greetings from the device
  pinMode(servoLedPin, OUTPUT);
  pinMode(ethernetLedPin, OUTPUT);
  pinMode(errorLedPin, OUTPUT);
  delay(500);
  digitalWrite(errorLedPin, HIGH);
  delay(500);
  digitalWrite(servoLedPin, HIGH);
  delay(500);
  digitalWrite(ethernetLedPin, HIGH);

  // Open serial communications
  Serial.begin(9600);
  Serial.println("\n      ( )");
  Serial.println("       H");
  Serial.println("       H");
  Serial.println("      _H_ ");
  Serial.println("   .-'-.-'-.");
  Serial.println("  /         \\");
  Serial.println(" |           |");
  Serial.println(" |   .-------'._");
  Serial.println(" |  / /  '.' '. \\");
  Serial.println(" |  \\ \\ @   @ / /");
  Serial.println(" |   '---------'");
  Serial.println(" |    _______|");
  Serial.println(" |  .'-+-+-+|");
  Serial.println(" |  '.-+-+-+|    Hello Peasants ");      
  Serial.println(" |    \"\"\"\"\"\" |");
  Serial.println(" '-.__   __.-'");
  Serial.println("      \"\"\"\n\n");

  delay(500);
  digitalWrite(ethernetLedPin, LOW);
  digitalWrite(servoLedPin, LOW);
  digitalWrite(errorLedPin, LOW);
  delay(500);

  // Connect to ethernet
  ethernetConnect();
  parseUrl();

/*  Serial.println("Attach to servo...");
  digitalWrite(servoLedPin, HIGH);
  myservo.attach(servoCtrlPin);
  digitalWrite(servoLedPin, LOW);*/

  Serial.println("IR Receiver initialize...");
  irrecv.enableIRIn();

}


void loop() {
  //read slaps from http
  int slaps = getHttpSlaps();
  if (slaps) {
    Serial.println("HTTP slap");
    doSlap(slaps);
  }

  int command = getIRCommand();
  if (command != 0) {
    switch (command) {
      case rc_btn_left:
      case rc_btn_right:
        (command == rc_btn_left) ? (++curDeg) : (--curDeg);
        Serial.print("degree adjusted to: ");
        Serial.println(curDeg);
        servoShaft(curDeg);
        break;
      case rc_btn_power:
        // start the Ethernet connection:
        Serial.println("Releasing and Renewing a DHCP Lease...");
        ethernetConnect();
        break;
      case rc_btn_0:
        curDeg = initDeg;
        Serial.print("degree reseted to: ");
        Serial.println(curDeg);
        servoShaft(curDeg);
        break;
      case rc_btn_1:
        Serial.println("IR slap 1");
        doSlap(1);
        break;
      case rc_btn_2:
        Serial.println("IR slap 2");
        doSlap(2);
        break;
      case rc_btn_3:
        Serial.println("IR slap 3");
        doSlap(3);
        break;
      case rc_btn_4:
        Serial.println("IR slap 4");
        doSlap(4);
        break;
    }
    // avoid http "interruption" during remote control
    lastConnectionTime = millis();
    irrecv.resume(); //avoid long press repetition
  }

  // if ten seconds have passed since last servo shaft,
  // then let's detach from servo
  if ((millis() - lastServoShaftTime > postingInterval)
      && myservo.attached()) {
    Serial.println("Detach from servo...");
    myservo.detach();
    digitalWrite(servoLedPin, LOW);
  }

}
