#include <WiFi.h>
#include <WiFiMulti.h>
#include <OneWire.h>  // DS18B20
#include <DallasTemperature.h>
#include <MQUnifiedsensor.h>  // MQ3
#include <HCSR04.h>

#define SOUND_SPEED 0.034  //HC-SR04
#define Board ("ESP32")
#define Pin (33)       //MQ3-PIN
#define Type ("MQ-3")  //MQ3
#define Voltage_Resolution (3.3)
#define ADC_Bit_Resolution (12)  // For ESP32
#define RatioMQ3CleanAir (9.83)  //RS / R0 = 9.83 ppm
#define INFO_CNT 8
#define CMD_SIZE 100

void Wifi_init();
void Upload_Info();
void MQ3_init();
void Timer_init();
void sendCode();

byte state;
char* myInfo[INFO_CNT]={"",};

//Wifi
WiFiMulti WiFiMulti;
WiFiClient client;
WiFiClient worker;
const uint16_t port = 5001;
const char* host = "10.10.141.4";  // ip or dns

//HC-SR04
const int trigPin = 5;     // trig HC-SR04
const int echoPin = 18;    // echo HC-SR04
long duration;
float distance;

//MQ3
const int oneWireBus = 4;  //DS18B20 PIN
double alcoholPPM = (33);  // MQ3 PIN
OneWire oneWire(oneWireBus);
MQUnifiedsensor MQ3(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);

//timer
volatile int intCnt;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//temp
float temp;
DallasTemperature tempSensor(&oneWire);

void IRAM_ATTR onTimer(){
  portENTER_CRITICAL_ISR(&timerMux);
  intCnt++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  Serial.begin(115200);
  Wifi_init();
  Upload_Info();
  Serial.println();
  HCSR04.begin(trigPin, echoPin);  //ultrasonic sensor,
  tempSensor.begin();                 //temperature sensore
  MQ3_init(); // alchol sensor
  Timer_init();
}

void loop() {
  readSensor();
  if(intCnt>0)
  {
    portENTER_CRITICAL(&timerMux);
    intCnt=0;
    portEXIT_CRITICAL(&timerMux);
    sendCode();
  }   
}

void MQ3_init() {
  MQ3.setRegressionMethod(1);  //_PPM =  a*ratio^b
  MQ3.setA(0.3934);
  MQ3.setB(-1.504);  // Configurate the ecuation values to get Alcohol concentration

  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ3.update();  // Update data, the arduino will be read the voltage on the analog pin
    calcR0 += MQ3.calibrate(RatioMQ3CleanAir);
    Serial.print(".");
  }
  MQ3.setR0(calcR0 / 10);
  Serial.println("  done!.");

  if (isinf(calcR0)) {
    Serial.println("Warning: Conection issue founded, R0 is infite (Open circuit detected) please check your wiring and supply");
    while (1)
      ;
  }
  if (calcR0 == 0) {
    Serial.println("Warning: Conection issue founded, R0 is zero (Analog pin with short circuit to ground) please check your wiring and supply");
    while (1)
      ;
  }
}

void Wifi_init() {
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
  client.print("[Hel0:PASSWD]\n");
}

void Upload_Info()
{
  int i = 0;
  char * pToken;
  char * pArray[INFO_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;
  if(client.available()){
    while(1){
      len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
      client.flush();
      Serial.print("Waiting_Info...\n");
      Serial.println(recvBuf);

      if(!strncmp(recvBuf,"[Inf",3))
      {
          pToken = strtok(recvBuf, "[@]");
        while (pToken != NULL)
        {
          pArray[i] =  pToken;
          if (++i >= INFO_CNT)
            break;
          pToken = strtok(NULL, "[@]");
        } 
        if(!strncmp(pArray[0],"Inf",3))
        {
            for(int row=0; row<INFO_CNT; row++)
          {
            myInfo[row]=pArray[row];
          }
          Serial.println(myInfo[3]);
          worker.connect(host, port);
          worker.printf("[%s_Hel:PASSWD]\n",myInfo[3]);
          delay(500);      
          break;
        }
      }            
    }
  } 
  delay(500); 
}

void Timer_init(){
  timer = timerBegin(0,80,true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 300000, true);
  timerAlarmEnable(timer);
}

void readSensor()
{
  tempSensor.requestTemperatures();
  temp = tempSensor.getTempCByIndex(0);
  MQ3.update();
  alcoholPPM = MQ3.readSensor();

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * SOUND_SPEED / 2;

  if (temp <= 25.3 || distance > 3 || alcoholPPM > 0.03) {
    state = 0;
  }
  else{
    state = 1;
  }
}

void sendCode()  
{
  Serial.print("temp: ");
  Serial.print(temp);
  Serial.println("ºC");
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print("Alcohol now (PPM): ");
  Serial.println(alcoholPPM);
  if(state==1)
  { 
    Serial.println("\nWarning!");
    worker.print("\n[CODE]0");
  }
  else{
    Serial.println("\nOK!");
    worker.print("\n[CODE]1");
  }
}