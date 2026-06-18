#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

//================ WIFI ==================
const char* ssid = "Bui";
const char* password = "12341234";

//================ PIN ===================
#define DHT_PIN 25
#define DHTTYPE DHT11
#define MQ2_PIN 35
#define LED_GREEN 23
#define LED_YELLOW 22
#define LED_RED 19
#define BUZZER 4

//================ SENSOR =================
DHT dht(DHT_PIN, DHTTYPE);
WebServer server(80);

//================ THRESHOLD ==============
const int GAS_WARNING = 300;
const int GAS_DANGER  = 500;
const float TEMP_WARNING = 35.0;
const float TEMP_DANGER  = 40.0; // Ngưỡng nhiệt độ Nguy hiểm mới

//================ DATA ===================
float currentTemp = 0;
float currentHum  = 0;
int currentGas    = 0;
String currentStatus = "Khoi dong...";
int statusCode = 0;

//================ TIMER ==================
unsigned long previousMillis = 0;
const unsigned long interval = 2000;

//================ HTML ===================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>ESP32 Dashboard</title>
    <style>
        body{font-family:Arial;background:#f4f7fc;padding:20px;}
        .card{background:white;padding:20px;margin:10px;border-radius:15px;box-shadow:0 0 10px rgba(0,0,0,.1);}
        .value{font-size:40px;font-weight:bold;}
        .safe{background:#d1fae5;color:#065f46;}
        .warning{background:#fef3c7;color:#92400e;}
        .danger{background:#fee2e2;color:#991b1b;}
    </style>
</head>
<body>
    <h1>📡 ESP32 Smart Monitor</h1>
    <div id="status" class="card">Đang kết nối...</div>
    <div class="card"><h2>Nhiệt độ</h2><div class="value" id="temp">--</div></div>
    <div class="card"><h2>Độ ẩm</h2><div class="value" id="hum">--</div></div>
    <div class="card"><h2>MQ2 Gas</h2><div class="value" id="gas">--</div></div>
    <script>
        async function loadData(){
            try{
                let res = await fetch('/data?t=' + Date.now());
                let data = await res.json();
                document.getElementById("temp").innerHTML = data.temp + " °C";
                document.getElementById("hum").innerHTML = data.hum + " %";
                document.getElementById("gas").innerHTML = data.gas;
                let st = document.getElementById("status");
                st.innerHTML = data.status;
                st.className="card";
                if(data.statusCode==0) st.classList.add("safe");
                if(data.statusCode==1) st.classList.add("warning");
                if(data.statusCode==2) st.classList.add("danger");
            } catch(e){
                document.getElementById("status").innerHTML = "❌ Mất kết nối ESP32";
            }
        }
        setInterval(loadData,2000);
        loadData();
    </script>
</body>
</html>
)rawliteral";

//=========================================
void handleRoot() { server.send(200, "text/html", index_html); }

void handleData() {
    String json = "{\"temp\":" + String(currentTemp, 1) + 
                  ",\"hum\":" + String(currentHum, 1) + 
                  ",\"gas\":" + String(currentGas) + 
                  ",\"status\":\"" + currentStatus + "\"" +
                  ",\"statusCode\":" + String(statusCode) + "}";
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to IP: " + WiFi.localIP().toString());
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    
    dht.begin();
    analogReadResolution(12);
    analogSetPinAttenuation(MQ2_PIN, ADC_11db);
    
    connectWiFi();
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
    Serial.println("WebServer Started");
}

void loop() {
    server.handleClient();
    
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        float t = dht.readTemperature();
        float h = dht.readHumidity();
        if (!isnan(t)) currentTemp = t;
        if (!isnan(h)) currentHum = h;
        currentGas = analogRead(MQ2_PIN);
        
        // Logic mới: Đỏ và Còi kêu nếu Gas >= 500 HOẶC Nhiệt độ >= 40 độ
        if (currentGas >= GAS_DANGER || currentTemp >= TEMP_DANGER) {
            currentStatus = "NGUY HIEM: Co su co!";
            statusCode = 2;
            digitalWrite(LED_GREEN, LOW); 
            digitalWrite(LED_YELLOW, LOW); 
            digitalWrite(LED_RED, HIGH);
            digitalWrite(BUZZER, HIGH);
        } 
        // Logic cảnh báo: Vàng sáng nếu Gas >= 300 HOẶC Nhiệt độ >= 35 độ
        else if (currentGas >= GAS_WARNING || currentTemp >= TEMP_WARNING) {
            currentStatus = "CANH BAO";
            statusCode = 1;
            digitalWrite(LED_GREEN, LOW); 
            digitalWrite(LED_YELLOW, HIGH); 
            digitalWrite(LED_RED, LOW);
            digitalWrite(BUZZER, LOW);
        } 
        // Logic an toàn
        else {
            currentStatus = "AN TOAN";
            statusCode = 0;
            digitalWrite(LED_GREEN, HIGH); 
            digitalWrite(LED_YELLOW, LOW); 
            digitalWrite(LED_RED, LOW);
            digitalWrite(BUZZER, LOW);
        }
        
        Serial.printf("Temp=%.1f Hum=%.1f Gas=%d\n", currentTemp, currentHum, currentGas);
    }
}