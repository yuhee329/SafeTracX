/*
=================================
...MPU9250
SCL - D22
SDA - D21
INT - D2
VCC - 3.3V
GND
=================================
...LoRa 1276
RESET - D13
NES - D5
SCK - D18
MOSI - D23
MISO - D19
DIO0 - D4
VCC - 3.3V
GND
=================================
...KeyPad
R1 - D32
R2 - D33
R3 - D25
C1 - D26
C2 - D27
C3 - D14
=================================
*/

#include <MPU9250.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <SPI.h>
#include <LoRa.h>
#include <Keypad.h>

#define ss 5
#define rst 13
#define dio0 4
#define ARR_CNT 7
#define INFO_CNT 8
#define CMD_SIZE 100

void Wifi_init();
void Lora_init();
void IMU_init();
void Timer_init();
void Upload_Info();
void socketEvent();
void socketIMU();
void loraIMU();
void loraSend(char* Message);

MPU9250 IMU(i2c0, 0x68);
byte infoCheck=0;
char* myInfo[INFO_CNT]={0};
bool sendFlag=false;

//timer
volatile int intCnt;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//wifi
WiFiMulti WiFiMulti;
WiFiClient client;
WiFiClient worker;
int status;
int i = 0;
const uint16_t port = 5000;
const char* host = "10.10.141.69";  // ip or dns

//keypad...
const byte ROWS = 3;
const byte COLS = 3;
char hexaKeys[ROWS][COLS] = {
  {'0','1','2'},
  {'3','4','5'},
  {'6','7','8'}
};
byte rowPins[ROWS] = {32, 33, 25};
byte colPins[COLS] = {26, 27, 14}; 
Keypad pushKey = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
//ALLMSG,KMJ,KYH,JYH,RHJ

char* pushMsg[9]={
  "[KMJ]Report!\n",
  "[KYH]Report!\n",
  "[JYH]Report!\n",
  "[KMJ]Coming...\n",
  "[KYH]Coming...\n",
  "[JYH]Coming...\n",   
  "[ALLMSG]Warning!!!\n",
  "[ALLMSG]Help!!!\n",
  "[ALLMSG]119   !!!\n"
};
//...

void IRAM_ATTR onTimer(){
  portENTER_CRITICAL_ISR(&timerMux);
  intCnt++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  Serial.begin(115200);
  Wifi_init();
  Upload_Info();
  Lora_init();
  IMU_init();
  Timer_init();
  printf("SetUp Successed!\n");

#define RAD_TO_DEG 57.3
}

void loop() {  
  if(worker.available()){
    socketEvent();
  }   
  if(sendFlag){
    if(intCnt>0)
    {
      portENTER_CRITICAL(&timerMux);
      intCnt=0;
      portEXIT_CRITICAL(&timerMux); 
      socketIMU();    
    }
  }
  char button = pushKey.getKey(); 
  keyMsg(button);
  loraReceive(LoRa.parsePacket());      
}

void Wifi_init(){
  WiFiMulti.addAP("iot0", "iot00000");
  Serial.println();
  Serial.print("Waiting for WiFi... ");
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  client.connect(host, port);
  client.print("[VEST0:PASSWD]\n");
  delay(500);
}

void Upload_Info()
{
  int i = 0;
  static char * pInfo;
  static char recvInfo[CMD_SIZE] = {0};
  int len;
  if(client.available()){
    while(1){
      len = client.readBytesUntil('\n', recvInfo, CMD_SIZE);
      client.flush();
      Serial.print("Waiting_Info...\n");
      Serial.println(recvInfo);

      if(!strncmp(recvInfo,"[Inf",3))
      {
          pInfo = strtok(recvInfo, "[@]");
        while (pInfo != NULL)
        {
          myInfo[i] =  pInfo;
          if (++i >= INFO_CNT)
            break;
          pInfo = strtok(NULL, "[@]");
        } 
        if(!strncmp(myInfo[0],"Inf",3))
        {
          Serial.println(myInfo[3]);
          worker.connect(host, port);
          worker.printf("[%s:PASSWD]\n",myInfo[3]);                    
          delay(500);      
          break;
        }
      }            
    }
  } 
  delay(500); 
}

void Lora_init(){
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(9209E5)) { //920.9 MHz
    Serial.println("Starting LoRa failed!");    
    while (1);
  }
}

void IMU_init(){
    status = IMU.begin();
  if (status < 0) {
    Serial.println("IMU initialization unsuccessful");
    Serial.println("Check IMU wiring or try cycling power");
    Serial.print("Status: ");
    Serial.println(status);
    while (1) {
    }
    IMU.setAccelRange(MPU9250::ACCEL_RANGE_2G);
    IMU.setGyroRange(MPU9250::GYRO_RANGE_250DPS);
  }
}

void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  len = worker.readBytesUntil('\n', recvBuf, CMD_SIZE);
  worker.flush();
  Serial.print("recv : ");
  Serial.println(recvBuf);

  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  } 
  if(!strncmp(pArray[0],"Are",3))
  {
    sendFlag=true;
  }
}

void socketIMU()
{
  IMU.readSensor();     
  worker.printf("[logData]%.3f@%.3f@%.3f@%.3f@%.3f@%.3f", IMU.getAccelX_mss(), IMU.getAccelY_mss(), IMU.getAccelZ_mss(), IMU.getGyroX_rads(), IMU.getGyroY_rads(), IMU.getGyroZ_rads());
//    Serial.printf("Socket!!%.3f@%.3f@%.3f@%.3f@%.3f@%.3f\n", IMU.getAccelX_mss(), IMU.getAccelY_mss(), IMU.getAccelZ_mss(), IMU.getGyroX_rads(), IMU.getGyroY_rads(), IMU.getGyroZ_rads());
  worker.flush();
}

void loraReceive(int packetSize) {
  if (packetSize == 0) return; 

  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;
  String name;
  
  while (LoRa.available()) {
    len = LoRa.readBytesUntil('\n', recvBuf, CMD_SIZE);
  }
  pToken = strtok(recvBuf, "[]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[]");
  } 
  
  if(!strncmp(pArray[0],myInfo[3],3))
  {
    Serial.printf("<REC>%s\n",pArray[1]);    
    LoRa.beginPacket();
    LoRa.print("<RETURN>OK\n");
    LoRa.endPacket();
  }
  else if(!strncmp(pArray[0],"ALL",3))
  {
    Serial.printf("<REC>%s\n",pArray[1]);
    LoRa.beginPacket();
    LoRa.print("<RETURN>OK\n");
    LoRa.endPacket();    
  }
  else if(!strncmp(pArray[0],"<RE",3))
  {
    Serial.printf("%s\n",pArray[0]);  
  }
}

void loraSend(char* Message)
{
  LoRa.beginPacket();  
  LoRa.printf("%s", Message);
  LoRa.endPacket();
}

void Timer_init(){
  timer = timerBegin(0,80,true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 50000, true);
  timerAlarmEnable(timer);
}

void keyMsg(char button){
  switch(button)
  {
    case '0':
      worker.printf("%s",pushMsg[0]);
      Serial.printf("<SEND>%s",pushMsg[0]);
      loraSend(pushMsg[0]);
      break;
    case '1':
      worker.printf("%s",pushMsg[1]);
      Serial.printf("<SEND>%s",pushMsg[1]);
      loraSend(pushMsg[1]);
      break;
    case '2':
      worker.printf("%s",pushMsg[2]);
      Serial.printf("<SEND>%s",pushMsg[2]);
      loraSend(pushMsg[2]);
      break;
    case '3':
      worker.printf("%s",pushMsg[3]);
      Serial.printf("<SEND>%s",pushMsg[3]);
      loraSend(pushMsg[3]);
      break;
    case '4':
      worker.printf("%s",pushMsg[4]);
      Serial.printf("<SEND>%s",pushMsg[4]);
      loraSend(pushMsg[4]);
      break;
    case '5':
      worker.printf("%s",pushMsg[5]);
      Serial.printf("<SEND>%s",pushMsg[5]);
      loraSend(pushMsg[5]);
      break;
    case '6':
      worker.printf("%s",pushMsg[6]);
      Serial.printf("<SEND>%s",pushMsg[6]);
      loraSend(pushMsg[6]);
      break;
    case '7':
      worker.printf("%s",pushMsg[7]);
      Serial.printf("<SEND>%s",pushMsg[7]);
      loraSend(pushMsg[7]);
      break;
    case '8':
      worker.printf("%s",pushMsg[8]);
      Serial.printf("<SEND>%s",pushMsg[8]);
      loraSend(pushMsg[8]);
      break;
    default:
      break;
  }    
}