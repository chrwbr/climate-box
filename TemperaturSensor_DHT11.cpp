// OPL - Stand 23.06.20
// - Ventilatoren einpflegen
// - Relais ansteuern
// - Schalten der Relais nach Sensorabfrage für Temperatur (nach dem Hochladen der Daten)
// - Schalten der Relais nach Sensorabfrage für Feuchte (nach dem Hochladen der Daten)

/********************************************************************/
// libraries DHT11 (Temp+Feuchte)
#include "DHT.h" //DHT Bibliothek laden
#define DHTPIN 25   //Der Sensor wird an PIN 3 angeschlossen    
#define DHTTYPE DHT11    // Es handelt sich um den DHT11 Sensor
DHT dht(DHTPIN, DHTTYPE); //Der Sensor wird ab jetzt mit „dth“ angesprochen

// libraries Wlan-Connection (Code Momo)
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

// libraries TemperaturSensor
#include <OneWire.h>
#include <DallasTemperature.h>



/********************************************************************/
// Data wire is plugged into pin 33 on the ESP32
#define ONE_WIRE_BUS 33

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/********************************************************************/
// WiFif_Kennung (Code Momo angewendet auf Daten von Christian)
const char *ssid = "PROTOHAUS";
const char *password = "PH-Wlan-2016#";

const char *influxUrl = "http://openfarming.ai:8086/write?db=christian_weber";
const char *influxUser = "christian";
const char *influxPassword = "HTIRyAnv4RnPe5DC";

// Variable für Temperatur & Sollwertvorgabe [°C]
float temp[4];
int temp_soll = 28;     // Sollvorgabe der Temperatur
float temp_mean;        // Mittelwert aus den Temperaturen
float temp_tol = 1;     // Toleranz für Temperaturabweichung von temp_soll

// Variable für Abfragerate (1*60 = 1 Minute)
const int SleeptimeS = 1*60;
 
// String für Namen der einzelnen Temperatur Sensoren & DHTSENSOR
String addr[4];

// Namen der Relais zum Schalten des Peltier-Elements
int Relais_1A = 26;
int Relais_1B = 27;
int Relais_2A = 14;
int Relais_2B = 12;


// Ausschluss von 404-Seiten (Momo)
bool isHttpCodeOk(int httpCode)
{
  if (httpCode >= 200 && httpCode < 300)
  {
    return true;
  }
  else
  {
    return false;
  }
}

/********************************************************************/

void setup(void)
{
  // start serial port
  Serial.begin(9600);

  /********************************************************************/
  //Wifi-SetUp & Connection
  WiFi.persistent(false);
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /********************************************************************/
  // Start up the library
  sensors.begin(); //DHT11 Sensor starten
  dht.begin(); 

  // Output Pins für Relais definieren
  pinMode(Relais_1A, OUTPUT);
  pinMode(Relais_1B, OUTPUT);
  pinMode(Relais_2A, OUTPUT);
  pinMode(Relais_2B, OUTPUT);
}

void loop(void)
{
  /********************************************************************/
  // Benennung der einzelnen Sensoren
  addr[0] = "Feuchte_DHT";
  addr[1] = "Temperatur_DHT"; 
  addr[2] = "Temperatur_T1";
  addr[3] = "Temperatur_T2";

  //Abfrage und Ausgabe des DHT11-Sensors
  temp[0] = dht.readHumidity(); //die Luftfeuchtigkeit auslesen und unter „Luftfeutchtigkeit“ speichern
  
  temp[1] = dht.readTemperature();//die Temperatur auslesen und unter „Temperatur“ speichern
  
  Serial.print("Luftfeuchtigkeit: "); //Im seriellen Monitor den Text und 
  Serial.print(temp[0]); //die Dazugehörigen Werte anzeigen
  Serial.println(" %");
  Serial.print("Temperatur: ");
  Serial.print(temp[1]);
  Serial.println(" Grad Celsius");  
  /********************************************************************/
  // Request Sensor
  Serial.print(" Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperature readings
  Serial.println("DONE");

  /********************************************************************/
  // Abfrage der Temperaturen der einzelnen Sensoren über for-Schleife
  for(int i=0;i<=1;i++){
  //  addr[i] = "Temperatur T" +  String(i);
  
  // Request  & Print Temperatur
  temp[i+2] = sensors.getTempCByIndex(i); 
    Serial.println(String(i+2) + ". " + String(temp[i+2]) + ": " + addr[i+2]);

  delay(1000);
  }
  /********************************************************************/
  // Upload Temp
  if (WiFi.status() == WL_CONNECTED)
  { //Check WiFi connection status

    HTTPClient http; //Declare object of class HTTPClient

    // Send to InfluxDB
    Serial.println("Sending data to InfluxDB");

    String influxData = "";
    for (int i = 0; i < 4; i++)
    {
      if (temp[i] >= -127)
      {
        // Set data type and add tags
        influxData += "temperature,Klimakammer=air,id=" + addr[i];
        // Add temperature value and delimiter for next measurement
        influxData += " value=" + String(temp[i]) + "\n";
      }
    }
    influxData += "zero_value value=0";
    Serial.println(influxData);

    http.begin(influxUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setAuthorization(influxUser, influxPassword);

    Serial.println("Test1");

        int httpCode = -1; //Durch die Scleife wird der Upload solange versucht bis er klappt
    while(httpCode == -1){
     httpCode = http.POST(influxData);
   }    
    http.end();
    Serial.println("Hochladen abgeschlossen & Verbindung getrennt");
  } else {
    Serial.println("Error in WiFi connection");
    }

/********************************************************************/
// Regelung der Temperatur durch schalten der Relais



while(temp_mean < temp_soll - temp_tol) // || temp_mean > temp_soll + temp_tol)
{
    // Mittelwert aus allen Temperatursensoren
    temp_mean = (temp[1]+temp[2]+temp[3])/3;

    Serial.println("Einschalten der Relais");
    digitalWrite(Relais_1A, HIGH);
    digitalWrite(Relais_1B, HIGH);
    digitalWrite(Relais_2A, HIGH);
    digitalWrite(Relais_2B, HIGH);

    temp[1] = dht.readTemperature();
    temp[2] = sensors.getTempCByIndex(0);
    temp[3] = sensors.getTempCByIndex(1); 
    delay(10000);
}

  digitalWrite(Relais_1A, LOW);
  digitalWrite(Relais_1B, LOW);
  digitalWrite(Relais_2A, LOW);
  digitalWrite(Relais_2B, LOW);
  Serial.println("Ausschalten der Relais");




 

/********************************************************************/
    // Abfragedauer 1s = 1000000
  ESP.deepSleep(SleeptimeS * 1000000);
}


