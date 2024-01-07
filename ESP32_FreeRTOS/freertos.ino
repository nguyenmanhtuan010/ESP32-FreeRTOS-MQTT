#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h> 
#include <Wire.h> 
#include "DHT.h"
#include <Keypad.h>

#define ROW_NUM     4 
#define COLUMN_NUM  3 

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte pin_rows[ROW_NUM] = {19, 18, 5, 17};
byte pin_column[COLUMN_NUM] = {16, 0, 2}; 

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);
float ThresholdTemp;

#define RELAY1 12
#define RELAY2 13
#define MANU_BUTTON 33
#define AUTO_BUTTON 25
#define RELAY1_BUTTON 26
#define RELAY2_BUTTON 27
#define THRESHOLD_BUTTON 14
int MODE_STATE;

// DHT 11
#define DHTPin 4
#define DHTTYPE DHT22 
DHT dht(DHTPin, DHTTYPE);     

int totalColumns =16; // LCD 16x2
int totalRows = 2;
LiquidCrystal_I2C lcd(0x27, totalColumns, totalRows);

const char* ssid = "YOUR SSID";
const char* password = "YOUR PASSWORD";
const char* mqtt_server = "YOUR IP";

WiFiClient espClient;
PubSubClient client(espClient);

#define sub1 "temp"
#define sub2 "humi"
#define sub3 "sw1_state"
#define sub4 "sw2_state"
//kết nối wifi
void setup_Wifi(){
  WiFi.begin(ssid,password);
  while(WiFi.status()!= WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("đã kết nối wifi");
  Serial.println("ip address: ");
  Serial.println("WiFi.localIP()");
}
void reconnect(){
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-01";
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("sub1");
      client.subscribe("sub2");
      client.subscribe("sw1");
	  client.subscribe("sw2");
      client.subscribe("sub3");
      client.subscribe("sub4");
     } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callBack(char* topic, byte* payload, unsigned int lenght){
  Serial.print("tin nhan [");
  Serial.print(topic);
  Serial.print("]");
  for(int i = 0 ; i <lenght; i++){
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if(MODE_STATE==1){
    if(strcmp(topic, "sw1")==0)
    {
      if((char)payload[0] == '1') //on
        digitalWrite(RELAY1,HIGH);
      else if((char)payload[0] == '0') //off
        digitalWrite(RELAY1, LOW);
    }
	if(strcmp(topic, "sw2")==0)
    {
      if((char)payload[0] == '1') //on
        digitalWrite(RELAY2,HIGH);
      else if((char)payload[0] == '0') //off
        digitalWrite(RELAY2, LOW);
    }
  }
}

SemaphoreHandle_t xBinarySemaphore;
SemaphoreHandle_t xMutex;
QueueHandle_t xQueueTemp, xQueueHumi,xQueueTempMqtt,xQueueHumiMqtt;

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;

void TaskreadSensor( void *pvParameters );
void TaskChooseMode( void *pvParameters );
void TaskAutoMode( void *pvParameters );
void TaskManualMode( void *pvParameters );
void TaskRelay1On( void *pvParameters );
void TaskRelay1Off( void *pvParameters );
void TaskRelay2On( void *pvParameters );
void TaskRelay2Off( void *pvParameters );
void TaskSendSensorData(void *pvParameters);
void TaskUpdateThreshold(void *pvParameters);
void setup() {
  
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  
  setup_Wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callBack);
  dht.begin();
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" IoT With LCD ");
  lcd.setCursor(0,1);
  lcd.print("I2C Address: 0x27");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("NGUEN MANH TUAN");
  delay(1000);
  lcd.clear();

  pinMode(RELAY1,OUTPUT);
  pinMode(RELAY2,OUTPUT);
  pinMode(MANU_BUTTON,INPUT);
  pinMode(AUTO_BUTTON,INPUT);
  pinMode(RELAY1_BUTTON,INPUT);
  pinMode(RELAY2_BUTTON,INPUT);
  pinMode(THRESHOLD_BUTTON,INPUT);
  
  xQueueTemp = xQueueCreate(1,sizeof(float));
  xQueueHumi = xQueueCreate(1,sizeof(float));
  xQueueTempMqtt = xQueueCreate(1,sizeof(float));
  xQueueHumiMqtt = xQueueCreate(1,sizeof(float));

  vSemaphoreCreateBinary(xBinarySemaphore);
  xMutex = xSemaphoreCreateMutex();
  
  xTaskCreatePinnedToCore(
    TaskreadSensor
    ,  "ReadSensor"   
    ,  10000  
    ,  NULL
    ,  3  
    ,  &xHandle3 
    ,  ARDUINO_RUNNING_CORE);
    
  xTaskCreatePinnedToCore(
    TaskChooseMode
    ,  "ChooseMode"   
    ,  4096  
    ,  NULL
    ,  3  
    ,  NULL  
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskSendSensorData
    ,  "SendSensorData"   
    ,  4096  
    ,  NULL
    ,  1  
    ,  NULL  
    ,  ARDUINO_RUNNING_CORE); 
}

void loop()
{
  vTaskDelete(NULL);
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskreadSensor(void *pvParameters)  // This is a task.
{
  xSemaphoreTake(xMutex,portMAX_DELAY);
  for (;;) // A Task shall never return or exit.
  {
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    }
    else {
    Serial.print("Temperature: ");
    Serial.println(t);
    Serial.print("Humidity: ");
    Serial.println(h);
    }
    xSemaphoreGive(xMutex);
    xQueueSendToBack(xQueueTemp,&t,0);
    xQueueSendToBack(xQueueHumi,&h,0);
    xQueueSendToBack(xQueueTempMqtt,&t,0);
    xQueueSendToBack(xQueueHumiMqtt,&h,0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskChooseMode(void *pvParameters) {
  for (;;) {
    if (digitalRead(AUTO_BUTTON) == 0) {
      MODE_STATE = 0;
      // Create or resume Task4
      if (xHandle2 == NULL) {
        xTaskCreatePinnedToCore(
            TaskAutoMode,
            "AutoMode",
            4096,  // Stack size
            NULL,
            2,  // Priority
            &xHandle2,
            ARDUINO_RUNNING_CORE);
      }
      // Delete Task3 if it exists
      if (xHandle1 != NULL) {
        vTaskDelete(xHandle1);
        xHandle1 = NULL;
      }
    }
    
    if (digitalRead(MANU_BUTTON) == 0) {
      MODE_STATE = 1;
      // Create or resume Task3
      if (xHandle1 == NULL) {
        xTaskCreatePinnedToCore(
            TaskManualMode,
            "ManualMode",
            4096,  // Stack size
            NULL,
            2,  // Priority
            &xHandle1,
            ARDUINO_RUNNING_CORE);
      } 
      // Delete Task4 if it exists
      if (xHandle2 != NULL) {
        vTaskDelete(xHandle2);
        xHandle2 = NULL;
      }
    }

    Serial.println("TaskChooseMode running.....");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskManualMode(void *pvParameters) {
  float buff_t;
  float buff_h;
  for (;;) {
    xQueueReceive(xQueueTemp,&buff_t,portMAX_DELAY);
    xQueueReceive(xQueueHumi,&buff_h,portMAX_DELAY);
    if (digitalRead(RELAY1_BUTTON) == 0) {
      if(digitalRead(RELAY1) == 1){
        xTaskCreatePinnedToCore(
        TaskRelay1Off
        ,  "TurnOFF"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
      }
      else{
        xTaskCreatePinnedToCore(
        TaskRelay1On
        ,  "TurnON"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
      }     
    }
	
	if (digitalRead(RELAY2_BUTTON) == 0) {
      if(digitalRead(RELAY2) == 1){
        xTaskCreatePinnedToCore(
        TaskRelay2Off
        ,  "TurnOFF"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
      }
      else{
        xTaskCreatePinnedToCore(
        TaskRelay2On
        ,  "TurnON"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
      }     
    }
	
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("MANUAL MODE");
    lcd.setCursor(0,1);
    lcd.print("T:");
    lcd.print(buff_t);
    lcd.print("C ");
    lcd.print("H:");
    lcd.print(buff_h);
    lcd.print("%");
    Serial.println("TaskManualMode running.....");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskAutoMode(void *pvParameters) {
  float buff_t;
  float buff_h;
  xTaskCreatePinnedToCore(  
     TaskUpdateThreshold
     ,  "UpdateThreshold"
     ,  4096  
     ,  NULL
     ,  4  // Priority
     ,  &xHandle4  
     ,  ARDUINO_RUNNING_CORE);
  for (;;)
  {
    xQueueReceive(xQueueTemp,&buff_t,portMAX_DELAY);
    xQueueReceive(xQueueHumi,&buff_h,portMAX_DELAY);
    if(digitalRead(THRESHOLD_BUTTON) == 0) {
    vTaskResume(xHandle4);
      }
    if(buff_t > ThresholdTemp){
       xTaskCreatePinnedToCore(
        TaskRelay1On
        ,  "TurnON"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
    }
    else{
       xTaskCreatePinnedToCore(
        TaskRelay1Off
        ,  "TurnOFF"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
    }
	
	if(buff_h > ThresholdHumidity){
       xTaskCreatePinnedToCore(
        TaskRelay2On
        ,  "TurnON"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
    }
    else{
       xTaskCreatePinnedToCore(
        TaskRelay2Off
        ,  "TurnOFF"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("AUTO MODE  ");
    lcd.print(ThresholdTemp);
    lcd.setCursor(0,1);
    lcd.print("T:");
    lcd.print(buff_t);
    lcd.print("C ");
    lcd.print("H:");
    lcd.print(buff_h);
    lcd.print("%");
    Serial.println("TaskAutoMode running.....");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskRelay1On(void *pvParameters) {
    Serial.println("TaskRelay1On running.....");
    digitalWrite(RELAY1,HIGH);
    vTaskDelete(NULL);
}

void TaskRelay1Off(void *pvParameters) {
    Serial.println("TaskRelay1Off running.....");
    digitalWrite(RELAY1,LOW);
    vTaskDelete(NULL);
}

void TaskRelay2On(void *pvParameters) {
    Serial.println("TaskRelay2On running.....");
    digitalWrite(RELAY2,HIGH);
    vTaskDelete(NULL);
}

void TaskRelay2Off(void *pvParameters) {
    Serial.println("TaskRelay2Off running.....");
    digitalWrite(RELAY2,LOW);
    vTaskDelete(NULL);
}

void TaskUpdateThreshold(void *pvParameters) {
  char key;
  float userInputFloat[2] = {0.0, 0.0};
  int inputIndex = 0;
  for (;;) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Nhập ngưỡng:");
    
    do {
      key = keypad.getKey();
      if (key >= '0' && key <= '9') {
        userInputFloat[inputIndex] = userInputFloat[inputIndex] * 10 + (key - '0');
        if (userInputFloat[inputIndex] > 99.0) {
           userInputFloat[inputIndex] = 99.0;
        }
        lcd.setCursor(0, 1);
        lcd.print(userInputFloat[inputIndex]);
      }
      if (key == '#') {
        inputIndex++;
        if (inputIndex >= 2) {
          inputIndex = 0;
        }
      }
    } while (inputIndex < 2);
    
    ThresholdTemp = userInputFloat[0];
    ThresholdHumidity = userInputFloat[1];
    Serial.println("Đã cập nhật ngưỡng nhiệt độ: " + String(ThresholdTemp));
    Serial.println("Đã cập nhật ngưỡng độ ẩm: " + String(ThresholdHumidity));
    lcd.clear();
    vTaskSuspend(xHandle4);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskSendSensorData(void *pvParameters) {
  for (;;) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    float h;
    float t;
    int Relay1State = digitalRead(RELAY1);
	int Relay2State = digitalRead(RELAY2);
    xQueueReceive(xQueueHumiMqtt,&h,portMAX_DELAY);
    xQueueReceive(xQueueTempMqtt,&t,portMAX_DELAY);
    char tempString[8];
    dtostrf(t, 1, 2, tempString);
    char humiString[8];
    dtostrf(h, 1, 2, humiString);
    char Relay1String[8];
	char Relay2String[8];
    sprintf(Relay1String, "%d", Relay1State);
	sprintf(Relay2String, "%d", Relay2State);
    client.publish(sub1, tempString);
    client.publish(sub2, humiString);
    client.publish(sub3, Relay1String);
	client.publish(sub4, Relay2String);
    Serial.println("TaskSendSensorData running.....");
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  }
}
