/*  WEMOS D1 Mini
                     ______________________________
                    |   L T L T L T L T L T L T    |
                    |                              |
                 RST|                             1|TX HSer
                  A0|                             3|RX HSer
                  D0|16                           5|D1
                  D5|14                           4|D2
                  D6|12                    10kPUP_0|D3
RX SSer/HSer swap D7|13                LED_10kPUP_2|D4
TX SSer/HSer swap D8|15                            |GND
                 3V3|__                            |5V
                       |                           |
                       |___________________________|
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <InfluxDbClient.h>           //https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino



// WiFi Parameters
const char* ssid = "SSID";
const char* password = "SSID PASSWORD";
#define HOSTNAME "kamstrup382"     // Friedly hostname



// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "http://IP OF INFLUXDB server:8086"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
//#define INFLUXDB_TOKEN "server token"
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
//#define INFLUXDB_ORG "org id"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
//#define INFLUXDB_BUCKET "bucket name"
#define INFLUXDB_DB_NAME "DATABASE NAME"

#define INFLUXDB_USER "INFLUX DB USER"
#define INFLUXDB_PASSWORD "PASSWORD FOR INFLUXDB USER"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time:   "PST8PDT"
//  Eastern:        "EST5EDT"
//  Japanesse:      "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define WRITE_PRECISION WritePrecision::S
#define MAX_BATCH_SIZE 10
#define WRITE_BUFFER_SIZE 30

// InfluxDB client instance with preconfigured InfluxCloud certificate
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

// Number for loops to sync time using NTP
int iterations = 0;

#define READmulticalEVERY  1000  //1000                                                      //read multical every 2000ms
#define NBREG   13   


#define KAMBAUD 9600
// Kamstrup optical IR serial
#define KAMTIMEOUT 300  // Kamstrup timeout after transmit
//SoftwareSerial kamSer(PIN_KAMSER_RX, PIN_KAMSER_TX, false);  // Initialize serial
//SoftwareSerial kamSer;


typedef struct {
  float regvalarr;
  word const regarr;
  const String regtext;
} multical_struct;

//volatile
multical_struct multicalarr[NBREG] = {
  {0.00, 0x0001,"Energy_in"},                                                     //Wh
  {0.00, 0x0002,"Energy_out"},                                                    //Wh
  {0.00, 0x041e,"Voltage_L1"},                                                    //V
  {0.00, 0x041f,"Voltage_L2"},                                                    //V
  {0.00, 0x0420,"Voltage_L3"},                                                    //V
  {0.00, 0x0434,"Current_L1"},                                                    //A
  {0.00, 0x0435,"Current_L2"},                                                    //A 
  {0.00, 0x0436,"Current_L3"},                                                    //A
  {0.00, 0x0438,"Power_L1"},                                                      //kW
  {0.00, 0x0439,"Power_L2"},                                                      //kW
  {0.00, 0x043a,"Power_L3"},                                                      //kW
  {0.00, 0x03ff,"Current_Power"},                                                 //kW
  {0.00, 0x0027,"Max_Power"},                                                     //kW 
};

unsigned long readtime;
time_t ntpLastUpdate;
int ntpSyncTime = 3600;
bool read_done = false;

String lasterror = "";


void setup() {
  //Serial.begin(115200);                                                         //initialize serial
  
 // setup kamstrup serial
  //pinMode(PIN_KAMSER_RX,INPUT);
  //pinMode(PIN_KAMSER_TX,OUTPUT);
  Serial.begin(KAMBAUD,SERIAL_8N2);
  //kamSer.begin(KAMBAUD, SWSERIAL_8N2, PIN_KAMSER_RX, PIN_KAMSER_TX, false,256);
  // Setup wifi
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    //Serial.println("Connecting...");
  }
  
  // ***************************************************************************
  // Setup: MDNS responder
  // ***************************************************************************
  MDNS.begin(HOSTNAME);
  //Serial.print("Hostname: ");
  //Serial.print(HOSTNAME);

  ArduinoOTA.begin();


  // Set InfluxDB 1 authentication params
  client.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DB_NAME, INFLUXDB_USER, INFLUXDB_PASSWORD);
  
  // Sync time for certificate validation
  timeSync();

  // Check server connection
  if (client.validateConnection()) {
    //Serial.print("Connected to InfluxDB: ");
    //Serial.println(client.getServerUrl());
  } else {
    //Serial.print("InfluxDB connection failed: ");
    //Serial.println(client.getLastErrorMessage());
  }

  //Enable messages batching and retry buffer
  client.setWriteOptions(WRITE_PRECISION, MAX_BATCH_SIZE, WRITE_BUFFER_SIZE);


  
}

void loop() {
  // Sync time for batching once per hour
  if(time(nullptr)-ntpLastUpdate > ntpSyncTime) {//if (iterations++ >= 360) {
    timeSync();
    iterations = 0;
  }

  if (millis() - readtime >= READmulticalEVERY) {
    heatRead();
    readtime = millis();
  }

  if(read_done){
//put data to influx in buffer
    time_t tnow = time(nullptr);
    for (int i = 0; i < NBREG; i++) {
      Point heatMeter(HOSTNAME);
      //heatMeter.addTag("device", "Main");
      heatMeter.addTag("Type", multicalarr[i].regtext );
      //heatMeter.addTag("channel", String(WiFi.channel(i)));
      //heatMeter.addTag("open", String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN));
      heatMeter.addField("value", multicalarr[i].regvalarr);
      heatMeter.setTime(tnow);  //set the time

      // Print what are we exactly writing
      //Serial.print("Writing: ");
      //Serial.println(heatMeter.toLineProtocol());

      // Write point into buffer - low priority measures
      client.writePoint(heatMeter);
 
    }





  // End of the iteration - force write of all the values into InfluxDB as single transaction
  //Serial.println("Flushing data into InfluxDB");
  if (!client.flushBuffer()) {
    //Serial.print("InfluxDB flush failed: ");
    //Serial.println(client.getLastErrorMessage());
    //Serial.print("Full buffer: ");
    //Serial.println(client.isBufferFull() ? "Yes" : "No");
  }
  

  

  
    for(int i = 0; i < NBREG; i++) {
      //Serial.print(multicalarr[i].regtext);
      //Serial.print(" = ");
      //Serial.println(multicalarr[i].regvalarr);
    
    }
    read_done = false;
    
    
  }

  ArduinoOTA.handle();
  yield();
  /*Serial.println("Wait 10s");
  delay(10000);*/
}


//------------------------------------------------------------------------------

void timeSync() {
  // Synchronize UTC time with NTP servers
  // Accurate time is necessary for certificate validaton and writing in batches
  configTime(0, 0, "pool.ntp.org", "time.nis.gov");
  // Set timezone
  setenv("TZ", TZ_INFO, 1);

  // Wait till time is synced
  //Serial.print("Syncing time");
  int i = 0;
  while (time(nullptr) < 1000000000ul && i < 100) {
    //Serial.print(".");
    delay(100);
    i++;
  }
  //Serial.println();

  // Show time
  time_t tnow = time(nullptr);
  ntpLastUpdate = time(nullptr);
  //Serial.print("Synchronized time: ");
  //Serial.println(String(ctime(&tnow)));
}



//------------------------------------------------------------------------------
void heatRead() {

// poll the Kamstrup registers for data 
  for (int kreg = 0; kreg < NBREG; kreg++) {
    kamReadReg(kreg);
    delay(100);
  }
  read_done = true;
  
}


// kamReadReg - read a Kamstrup register
float kamReadReg(unsigned short kreg) {

  byte recvmsg[40];  // buffer of bytes to hold the received data
  float rval;        // this will hold the final value

  // prepare message to send and send it
  byte sendmsg[] = { 0x3f, 0x10, 0x01, (multicalarr[kreg].regarr >> 8), (multicalarr[kreg].regarr & 0xff) };
  kamSend(sendmsg, 5);

  // listen if we get an answer
  unsigned short rxnum = kamReceive(recvmsg);

  // check if number of received bytes > 0 
  if(rxnum != 0){
    
    // decode the received message
    rval = kamDecode(kreg,recvmsg);

    multicalarr[kreg].regvalarr = rval;
    // print out received value to terminal (debug)
    //Serial.print(kregstrings[kreg]);
    //Serial.print(": ");
    //Serial.print(rval);
    //Serial.print(" ");
    //Serial.println();
    
    return rval;
  }
}

// kamSend - send data to Kamstrup meter
void kamSend(byte const *msg, int msgsize) {

  // append checksum bytes to message
  byte newmsg[msgsize+2];
  for (int i = 0; i < msgsize; i++) { newmsg[i] = msg[i]; }
  newmsg[msgsize++] = 0x00;
  newmsg[msgsize++] = 0x00;
  int c = crc_1021(newmsg, msgsize);
  newmsg[msgsize-2] = (c >> 8);
  newmsg[msgsize-1] = c & 0xff;

  // build final transmit message - escape various bytes
  byte txmsg[20] = { 0x80 };   // prefix
  int txsize = 1;
  for (int i = 0; i < msgsize; i++) {
    if (newmsg[i] == 0x06 or newmsg[i] == 0x0d or newmsg[i] == 0x1b or newmsg[i] == 0x40 or newmsg[i] == 0x80) {
      txmsg[txsize++] = 0x1b;
      txmsg[txsize++] = newmsg[i] ^ 0xff;
    } else {
      txmsg[txsize++] = newmsg[i];
    }
  }
  txmsg[txsize++] = 0x0d;  // EOF

  // send to serial interface
  for (int x = 0; x < txsize; x++) {
    Serial.write(txmsg[x]);
  }

}

// kamReceive - receive bytes from Kamstrup meter
unsigned short kamReceive(byte recvmsg[]) {

  byte rxdata[50];  // buffer to hold received data
  unsigned long rxindex = 0;
  unsigned long starttime = millis();
  
  Serial.flush();  // flush serial buffer - might contain noise

  byte r;
  
  // loop until EOL received or timeout
  while(r != 0x0d){
    
    // handle rx timeout
    if(millis()-starttime > KAMTIMEOUT) {
      //Serial.println("Timed out listening for data");
      lasterror = "Timed out listening for data";
      return 0;
    }

    // handle incoming data
    if (Serial.available()) {

      // receive byte
      r = Serial.read();
      if(r != 0x40) {  // don't append if we see the start marker
        // append data
        rxdata[rxindex] = r;
        rxindex++; 
      }

    }
  }

  // remove escape markers from received data
  unsigned short j = 0;
  for (unsigned short i = 0; i < rxindex -1; i++) {
    if (rxdata[i] == 0x1b) {
      byte v = rxdata[i+1] ^ 0xff;
      if (v != 0x06 and v != 0x0d and v != 0x1b and v != 0x40 and v != 0x80){
        //Serial.print("Missing escape ");
        lasterror = "Missing escape ";
        //Serial.println(v,HEX);
      }
      recvmsg[j] = v;
      i++; // skip
    } else {
      recvmsg[j] = rxdata[i];
    }
    j++;
  }
  
  // check CRC
  if (crc_1021(recvmsg,j)) {
    //Serial.println("CRC error: ");
    lasterror = "CRC error: ";
    return 0;
  }
  
  return j;
  
}

// kamDecode - decodes received data
float kamDecode(unsigned short const kreg, byte const *msg) {

  // skip if message is not valid
  if (msg[0] != 0x3f or msg[1] != 0x10) {
    return false;
  }
  if (msg[2] != (multicalarr[kreg].regarr >> 8) or msg[3] != (multicalarr[kreg].regarr & 0xff)) {
    return false;
  }
    
  // decode the mantissa
  long x = 0;
  for (int i = 0; i < msg[5]; i++) {
    x <<= 8;
    x |= msg[i + 7];
  }
  
  // decode the exponent
  int i = msg[6] & 0x3f;
  if (msg[6] & 0x40) {
    i = -i;
  };
  float ifl = pow(10,i);
  if (msg[6] & 0x80) {
    ifl = -ifl;
  }

  // return final value
  return (float )(x * ifl);

}

// crc_1021 - calculate crc16
long crc_1021(byte const *inmsg, unsigned int len){
  long creg = 0x0000;
  for(unsigned int i = 0; i < len; i++) {
    int mask = 0x80;
    while(mask > 0) {
      creg <<= 1;
      if (inmsg[i] & mask){
        creg |= 1;
      }
      mask>>=1;
      if (creg & 0x10000) {
        creg &= 0xffff;
        creg ^= 0x1021;
      }
    }
  }
  return creg;
}
