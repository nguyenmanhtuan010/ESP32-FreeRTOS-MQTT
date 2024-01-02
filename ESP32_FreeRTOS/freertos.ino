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

#define FAN 12
#define MANU_BUTTON 25
#define AUTO_BUTTON 26
#define FAN_BUTTON 27
#define THRESHOLD_BUTTON 14
int MODE_STATE;

// DHT 11
#define DHTPin 4
#define DHTTYPE DHT22 
DHT dht(DHTPin, DHTTYPE);     

int totalColumns =16; // LCD 16x2
int totalRows = 2;
LiquidCrystal_I2C lcd(0x27, totalColumns, totalRows);

const char* ssid = "tun";
const char* password = "12345678";
const char* mqtt_server = "192.168.137.92";

WiFiClient espClient;
PubSubClient client(espClient);

#define sub1 "temp"
#define sub2 "humi"
#define sub3 "sw_state"

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
      client.subscribe("sw");
      client.subscribe("sub3");
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
    if(strcmp(topic, "sw")==0)
    {
      if((char)payload[0] == '1') //on
        digitalWrite(FAN,HIGH);
      else if((char)payload[0] == '0') //off
        digitalWrite(FAN, LOW);
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
void TaskFanOn( void *pvParameters );
void TaskFanOff( void *pvParameters );
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

  pinMode(FAN,OUTPUT);
  pinMode(MANU_BUTTON,INPUT);
  pinMode(AUTO_BUTTON,INPUT);
  pinMode(FAN_BUTTON,INPUT);
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
    if (digitalRead(FAN_BUTTON) == 0) {
      if(digitalRead(FAN) == 1){
        xTaskCreatePinnedToCore(
        TaskFanOff
        ,  "TurnOFF"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
      }
      else{
        xTaskCreatePinnedToCore(
        TaskFanOn
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
    if(buff_t < ThresholdTemp){
       xTaskCreatePinnedToCore(
        TaskFanOn
        ,  "TurnON"
        ,  4096  // Stack size
        ,  NULL
        ,  4  // Priority
        ,  NULL 
        ,  ARDUINO_RUNNING_CORE);
    }
    else{
       xTaskCreatePinnedToCore(
        TaskFanOff
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

void TaskFanOn(void *pvParameters) {
    Serial.println("TaskFanOn running.....");
    digitalWrite(FAN,HIGH);
    vTaskDelete(NULL);
}

void TaskFanOff(void *pvParameters) {
    Serial.println("TaskFanOff running.....");
    digitalWrite(FAN,LOW);
    vTaskDelete(NULL);
}

void TaskUpdateThreshold(void *pvParameters) {
  char key;
  for (;;) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Threshold:");
      float userInputFloat = 0.0; 
      
      do {
        key = keypad.getKey();
        if (key >= '0' && key <= '9') {
          userInputFloat = userInputFloat * 10 + (key - '0');
          if (userInputFloat > 99.0) {
             userInputFloat = 99.0;
          }
          lcd.setCursor(0, 1);
          lcd.print( userInputFloat);
        }
      } while (key != '#');   
      ThresholdTemp = userInputFloat;
      Serial.println("Updated ThresholdTemp: " + String(ThresholdTemp));
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
    int fanState = digitalRead(FAN);
    xQueueReceive(xQueueHumiMqtt,&h,portMAX_DELAY);
    xQueueReceive(xQueueTempMqtt,&t,portMAX_DELAY);
    char tempString[8];
    dtostrf(t, 1, 2, tempString);
    char humiString[8];
    dtostrf(h, 1, 2, humiString);
    char fanString[8];
    sprintf(fanString, "%d", fanState);
    client.publish(sub1, tempString);
    client.publish(sub2, humiString);
    client.publish(sub3, fanString);
    Serial.println("TaskSendSensorData running.....");
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  }
}
