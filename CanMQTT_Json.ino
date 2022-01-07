#include <creacionEnviosMQTTyCAN.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32CAN.h>
#include <CAN_config.h>
#include <iostream>
#include <ArduinoJson.hpp>
#include <ArduinoJson.h>

using namespace std;


// Update these with values suitable for your network.
const char* ssid = "RED ACCESA";
const char* password = "037E32E7";

const char* mqtt_server = "143.110.229.128";  // Serivor remoto
//const char* mqtt_server = "192.168.1.115";  // Servidor local

// topic_basic = Accesa/project/ratify  // confirmar
// topic_basic = Accesa/project/admit   //  recibir
String rootTopic = "Accesa";
String project = "laboratory";

String topicPubString = rootTopic + "/"+ project +"/ratify";
String topicSubString = rootTopic + "/"+ project +"/admit";
String clienteIDString = rootTopic + "_"+ project +"_admit" + random(1,100);

char path[] = "/";                    //no tiene otras direcciones 
const char* topic_sub = topicSubString.c_str();
const char* topic_pub = topicPubString.c_str();
const char* clienteID = clienteIDString.c_str();
/*
 *  OBJETOS
*/
EnvioCAN         CAN;
CAN_frame_t      rx_frame;
CAN_device_t     CAN_cfg;
String json;

//WiFiClient       client;

EnvioMqtt   cadenaMqtt;

/*
 * FUNCIONES PROTOTIPO
*/
void envioCAN(String cadenaTCP);

/*
 * VARIABLES Mqtt
*/


WiFiClient espClient;
PubSubClient client(espClient);

/*
 * VARIABLES GLOBALES
*/
unsigned long previousMillis = 0;
const int interval = 1000;          // interval at which send CAN Messages (milliseconds)
const int rx_queue_size = 10;       // Receive Queue size
 
byte    limiteEnvioInst = 0;
bool    banderaEscInst = false, banderaEscDia = false, banderaEscFecha = false;
bool    banderaFechaFin = false, configurarEntrada = false;
String  dataEscenario, dataConfiguracion;
char    dataSnd[15];

long lastMsg = 0;
char msg[50];
int value = 0;

/*
 * ********************************************************************
 *                                 Setup
 * ********************************************************************
*/

void setup() {

  Serial.begin(115200);
  //Serial.begin(92600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.publish(topic_pub,"Mensaje Recibido");
  
  /*
   * Conexion CAN
  */
  /* Seleccionar pines TX, RX y baudios */
  CAN_cfg.speed=CAN_SPEED_125KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_5;
  CAN_cfg.rx_pin_id = GPIO_NUM_35;
  
  /* Crear una cola de 10 mensajes en el buffer del CAN */
  CAN_cfg.rx_queue = xQueueCreate(300,sizeof(CAN_frame_t));
  
  //INICIALIZAR MODULO CAN
  ESP32Can.CANInit();
}

/*
 * ********************************************************************
 *                              Setup WIFI
 * ********************************************************************
*/

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
/*
 * ********************************************************************
 *                              Reconectado
 * ********************************************************************
*/

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clienteID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic_pub, "Enviando el primer mensaje");

      client.subscribe(topic_sub);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
  
      delay(5000);
    }
  }

}
/*
 * ********************************************************************
 *                              CallBack
 * ********************************************************************
*/

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  
  char MQTT_to_CAN[length] ;
  String payload_string = "";
  String Lampara_Encender = "";
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    Lampara_Encender = Lampara_Encender + (char)payload[i];
    
  }
  Serial.println();

  //Deserializacion de Json y envio a red CAN
  DeserializeObject(Lampara_Encender);

}

/*
 * ********************************************************************
 *                                   Envio CAN
 * ********************************************************************
*/

void envioCAN(String cadenaTCP){

  
  char destinatario[3];
  byte destinatarioByte;
  
  for(byte i=0; i<2; i++) destinatario[i] = cadenaTCP[3+i];
  destinatarioByte = CAN.x2i(destinatario);
  rx_frame.FIR.B.FF = CAN_frame_ext;
  rx_frame.MsgID = destinatarioByte;
  rx_frame.FIR.B.DLC = 8;  
  
  //ENVIO CAN
  ESP32Can.CANWriteFrame(&rx_frame);
  
}

/*
 * ************************************************************************************************
 *                                          JSON
 * ************************************************************************************************
*/
//Serializacion
void SerializeObject(String json) {
    String dataSnd_string;
    char buffer[256];
    dataSnd_string = String(dataSnd);
    StaticJsonDocument<300> doc;
    doc["can"]        = (dataSnd_string.substring(3,5));
    doc["pin"]        = (dataSnd_string.substring(5,6));
    doc["percentage"] = (dataSnd_string.substring(6,9));
    doc["rgb"]        = (dataSnd_string.substring(11,14));

    size_t n = serializeJson(doc, buffer);
    client.publish(topic_pub, buffer, n);
}

// Deserializacion 
void DeserializeObject(String dato_json) {   
  //String envio_json_can = "{\"can\":\"09\",\"pin\":\"1\",\"stauts\":true,\"percentage\":100,\"rgb\":\"xxx\"}";
  String envio_json_can = dato_json;
    StaticJsonDocument<300> doc;
    
    DeserializationError error = deserializeJson(doc, envio_json_can);
    if (error) { return; }
    String can        = doc["can"];
    String pin        = doc["pin"];
    String percentage = doc["percentage"];
    String rgb        = doc["rgb"];

    
    envio_json_can = "FF1"+ can + pin + percentage + "00" + rgb;
    Serial.println("------------------json desarmado------------------");
    Serial.println(envio_json_can);


    
    CAN.envioActivacion(envio_json_can, &rx_frame);
    envioCAN(envio_json_can);
    
}

/*
 * ********************************************************************
 * Programa principal
 * ********************************************************************
*/

void loop() {
  String data;
  String json_ouput;
  
  /*
   * MENSAJES RECIBIDOS POR EL BUS CAN
   */
  if(xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3*portTICK_PERIOD_MS)==pdTRUE){
      
      if(rx_frame.FIR.B.FF==CAN_frame_std)
        printf("New standard frame");
      else
        printf("NUEVO MENSAJE CAN     ");

      if(rx_frame.FIR.B.RTR==CAN_RTR)
        printf(" RTR from 0x%.3x, DLC %d\r\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);
      else{
        printf("ID CAN: 0x%.3x, No. BYTES: %d\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);
        Serial.print(char(rx_frame.data.u8[0]));Serial.print("|");
        Serial.print(rx_frame.data.u8[1]);Serial.print("|");
        Serial.print(rx_frame.data.u8[2]);Serial.print("|");
        Serial.print(rx_frame.data.u8[3]);Serial.print("|");
        Serial.print(rx_frame.data.u8[4]);Serial.print("|");
        Serial.print(rx_frame.data.u8[5]);Serial.print("|");
        Serial.print(rx_frame.data.u8[6]);Serial.print("|");
        Serial.println(rx_frame.data.u8[7]);
        if(rx_frame.MsgID == 255 || 1){
          switch(char(rx_frame.data.u8[0])){
            case '0': 
                      Serial.println("ACTIVACIÓN/DESACTIVACIÓN CONFIRMADA");
                      cadenaMqtt.envioActivacion(&rx_frame, dataSnd);
                      client.publish(topic_pub, dataSnd);
                      
                      // Transformacion y envio de Json
                      SerializeObject(dataSnd);

                      
                      break;
            
            default:  break;       
          }
        memset(dataSnd, 'x', 15);
        printf("\n");
        }
      }
  }

/*
 * ********************************************************************
 * Envio  MQTT
 * ********************************************************************
*/

  if (!client.connected()) {  
    reconnect();  
  }
  client.loop();
  long now = millis(); 
}
