#include <WiFi.h>
#include <WiFiMulti.h>
#include <SPI.h>      // SPI 라이브러리
#include <MFRC522.h>  //RFID 라이브러리

#define RST_PIN 14  // RST9번핀 (리셋 핀)
#define SS_PIN 5    // SS10번핀 (spi통신을 위한 ss핀)
#define ARR_CNT 7
#define CMD_SIZE 100

void send_Data(byte i);
void socketEvent();
void RFID_Check();
void print_RFprofile();
void CheckCondition();
void Wifi_init();

byte IndexNumber = 0;  //match ? 1:0 //global
WiFiMulti WiFiMulti;
WiFiClient client;
int status;
int i = 0;
char num[10]={0}; 
byte len;
const uint16_t port = 5001;
const char *host = "10.10.141.69";  // ip or dns
bool sendFlag = false;

byte key[4] = { 0, 0, 0, 0 };  //input_RFID_KEY_VALUE

char *ptr0[7] = { "None", "None", "None", "None", "None", "None", "None" };  //Dismatch_ID_INFORMATION
char *ptr1[7] = { "PPE", "ID", "JYH", "30", "177", "73", "AB" };             //Frist_ID_INFORMATION (PPE/Id/Name/Age/Height/Weight/Blood)
char *ptr2[7] = { "PPE", "ID", "KYH", "21", "161", "51", "B" };              //Frist_ID_INFORMATION (PPE/Id/Name/Age/Height/Weight/Blood)
char *ptr3[7] = { "PPE", "ID", "RHJ", "22", "162", "52", "O" };              //Frist_ID_INFORMATION (PPE/Id/Name/Age/Height/Weight/Blood)
char *ptr4[7] = { "PPE", "ID", "KMJ", "20", "160", "50", "A" };              //Frist_ID_INFORMATION (PPE/Id/Name/Age/Height/Weight/Blood)
char *ptr5[7], *ptr6[7], *ptr7[7], *ptr8[7], *ptr9[7], *ptr10[7],
  *ptr11[7], *ptr12[7], *ptr13[7], *ptr14[7];                                                                            //Blank_INFORMATION(MAX=15)
char **profile[15] = { ptr0, ptr1, ptr2, ptr3, ptr4, ptr5, ptr6, ptr7, ptr8, ptr9, ptr10, ptr11, ptr12, ptr13, ptr14 };  //INFORMATION_LIST
byte key0[4] = { 0, 0, 0, 0 };                                                                                           //Error_RFID_KEY_VALUE
byte key1[4] = { 77, 123, 4, 50 };                                                                                       //First_RFID_KEY_VALUE
byte key2[4], key3[4], key4[4], key5[4], key6[4], key7[4], key8[4], key9[4], key10[4],
  key11[4], key12[4], key13[4], key14[4];                                                                                 //Blank_RFID_KEY_VALUE
byte *key_index[15] = { key0, key1, key2, key3, key4, key5, key6, key7, key8, key9, key10, key11, key12, key13, key14 };  //RFID_LIST

MFRC522 mfrc522(SS_PIN, RST_PIN);  //

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  SPI.begin();
  Wifi_init();
  mfrc522.PCD_Init(SS_PIN, RST_PIN);
}

void loop() {
  if(Serial.available())
  {
    len = Serial.readBytesUntil('\n', num, 10);
  }  
  if(client.available())
  {
    socketEvent();
  }
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {  //새로운 카드가 인식되거나 읽힌 RFC_Data가 있는지 확인
    delay(500);
  }
  if(sendFlag){
    RFID_Check();
  }
  else{
    Serial.println("Waiting for wearing check");
  }
  Serial.println();
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
  client.print("[Info:PASSWD]\n");
}


void RFID_Check() {
  byte match1, match2, match3, match4 = 0;  //Inforamtion에 저장되어 있는 RFID_KEY_VALUE

  byte length = sizeof(key_index) / sizeof(key_index[0]);  //=15

  Serial.print("Card UID:");

  for (byte i = 0; i < 4; i++) {  //store RFID_KEY_VALUE
    Serial.print(mfrc522.uid.uidByte[i]);
    Serial.print(" ");
    key[i] = mfrc522.uid.uidByte[i];
  }

  Serial.println();  //몇 번째 사용자의 RFID_KEY_VALUE인지 판별
  for (int i = 0; i < length; i++) {
    if (key[0] == key_index[i][0]) match1 = 1;
    else match1 = 0;
    if (key[1] == key_index[i][1]) match2 = 1;
    else match2 = 0;
    if (key[2] == key_index[i][2]) match3 = 1;
    else match3 = 0;
    if (key[3] == key_index[i][3]) match4 = 1;
    else match4 = 0;
    if (match1 * match2 * match3 * match4 == 1) {
      IndexNumber = i;
      break;
    }
  }
  send_Data(IndexNumber);
}

void send_Data(byte i) {
  Serial.printf("[VEST%s]%s@%s@%s@%s@%s@%s@%s\n",num,profile[i][0],profile[i][1],profile[i][2],profile[i][3],profile[i][4],profile[i][5],profile[i][6]);
  Serial.printf("[HEL%s]%s@%s@%s@%s@%s@%s@%s\n",num,profile[i][0],profile[i][1],profile[i][2],profile[i][3],profile[i][4],profile[i][5],profile[i][6]);
  client.printf("[VEST%s]%s@%s@%s@%s@%s@%s@%s\n",num,profile[i][0],profile[i][1],profile[i][2],profile[i][3],profile[i][4],profile[i][5],profile[i][6]);
  client.printf("[HEL%s]%s@%s@%s@%s@%s@%s@%s\n",num,profile[i][0],profile[i][1],profile[i][2],profile[i][3],profile[i][4],profile[i][5],profile[i][6]);
}

void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
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
  if(!strncmp(pArray[0],"Hel",3))
  {
    sendFlag=true;
  }
}