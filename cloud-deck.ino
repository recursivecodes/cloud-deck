/*
 *  Project     'Stream Cheap' Mini Macro Keyboard
 *  @author     David Madison
 *  @link       partsnotincluded.com/electronics/diy-stream-deck-mini-macro-keyboard
 *  @license    MIT - Copyright (c) 2018 David Madison
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include "creds.h"
#include "ArduinoJson.h"
#include "mbedtls/base64.h"
#include "oci.h"

// Pin definitions
#define BUTTON_PIN1 13
#define BUTTON_PIN2 12
#define BUTTON_PIN3 14
#define BUTTON_PIN4 27
#define BUTTON_PIN5 26
#define BUTTON_PIN6 25
#define BUTTON_PIN7 33
#define BUTTON_PIN8 32

OciProfile ociProfile(tenancyOcid, userOcid, keyFingerprint, apiKey);
Oci oci(ociProfile);

void toggleQaVm(bool start) {
  char vmPath[200] = "/20160918/instances/";
  strcat(vmPath, qaInstanceOcid);
  strcat(vmPath, "?action=");
  strcat(vmPath, start ? "START" : "STOP");
    
  OciApiRequest vmRequest(iaasHost, vmPath, oci.HTTP_METHOD_POST, {}, 0, NULL);
  OciApiResponse vmResponse;
  oci.apiCall(vmRequest, vmResponse);
  if( vmResponse.statusCode == 200 ) {
    StaticJsonDocument<200> vmFilter;
    vmFilter["displayName"] = true;
    vmFilter["lifecycleState"] = true;
    
    DynamicJsonDocument vmDoc(300);
    deserializeJson(vmDoc, vmResponse.response, DeserializationOption::Filter(vmFilter));
    //Serial.println("VM State Response:");
    //serializeJsonPretty(vmDoc, Serial);  

    char vmMessage[200] = "*Updated VM:* \\n";
    strcat(vmMessage, "Display Name: ");
    strcat(vmMessage, vmDoc["displayName"]);
    strcat(vmMessage, "\\n");
    strcat(vmMessage, "State: ");
    strcat(vmMessage, vmDoc["lifecycleState"]);
    strcat(vmMessage, "\\n");
    {
      sendNotification((char*) vmMessage);
    }
  }
  else {
    Serial.println(vmResponse.statusCode);
    Serial.println(vmResponse.errorMsg);
  }
}

void toggleQaDb(bool start) {
  
  char dbPath[200] = "/20160918/autonomousDatabases/";
  strcat(dbPath, dbOcid);
  strcat(dbPath, "/actions/");
  strcat(dbPath, start ? "start" : "stop");
  
  OciApiRequest dbRequest(dbHost, dbPath, oci.HTTP_METHOD_POST, {}, 0, NULL);
  OciApiResponse dbResponse;
  oci.apiCall(dbRequest, dbResponse);
  if( dbResponse.statusCode == 200 ) {
    StaticJsonDocument<200> dbFilter;
    dbFilter["displayName"] = true;
    dbFilter["dbName"] = true;
    dbFilter["lifecycleState"] = true;
    
    DynamicJsonDocument dbDoc(300);
    deserializeJson(dbDoc, dbResponse.response, DeserializationOption::Filter(dbFilter));
    //Serial.println("DB State Response:");
    //serializeJsonPretty(dbDoc, Serial);  

    char dbMessage[200] = "*Updated DB:* \\n";
    strcat(dbMessage, "Display Name: ");
    strcat(dbMessage, dbDoc["displayName"]);
    strcat(dbMessage, "\\n");
    strcat(dbMessage, "DB Name: ");
    strcat(dbMessage, dbDoc["dbName"]);
    strcat(dbMessage, "\\n");
    strcat(dbMessage, "State: ");
    strcat(dbMessage, dbDoc["lifecycleState"]);
    strcat(dbMessage, "\\n");
    {
      sendNotification((char*) dbMessage);
    }
  }
  else {
    if(dbResponse.statusCode == 409) {
      char dbErr[100] = "Failed to ";
      strcat(dbErr, start ? "start" : "stop");
      strcat(dbErr, " DB. Already ");
      strcat(dbErr, start ? "started." : "stopped.");
      sendNotification((char*) dbErr);
    }
    Serial.println(dbResponse.statusCode);
    Serial.println(dbResponse.errorMsg);
  }
}
void listInstances() {
  char instancePath[200] = "/20160918/instances?limit=10&sortBy=TIMECREATED&compartmentId=";
  strcat(instancePath, compartmentOcid);
  
  OciApiRequest instanceRequest(iaasHost, instancePath, oci.HTTP_METHOD_GET, {}, 0, NULL);
  OciApiResponse instanceResponse;
  oci.apiCall(instanceRequest, instanceResponse);
  if( instanceResponse.statusCode == 200 ) {

    StaticJsonDocument<300> filter;
    filter[0]["displayName"] = true;
    //filter[0]["compartmentId"] = true;
    filter[0]["shape"] = true;
    filter[0]["id"] = true;
    filter[0]["lifecycleState"] = true;
    DynamicJsonDocument doc(2000);
    deserializeJson(doc, instanceResponse.response, DeserializationOption::Filter(filter));
    //Serial.println("List Instance Response:");
    //serializeJsonPretty(doc, Serial);  

    JsonArray arr = doc.as<JsonArray>();

    char message[] = "*VM List (Max: 10 Recent):* \\n";
    sendNotification(message);
    
    for (JsonVariant value : arr) {
      char vm[200] = "";
      strcat(vm, "Name: ");
      strcat(vm, value["displayName"].as<char*>());
      strcat(vm, "\\nOCID: ");
      strcat(vm, value["id"].as<char*>());
      strcat(vm, "\\nShape: ");
      strcat(vm, value["shape"].as<char*>());
      strcat(vm, "\\nState: ");
      strcat(vm, value["lifecycleState"].as<char*>());
      strcat(vm, "\\n");
      sendNotification(vm);
    }
    
  }
  else {
    Serial.println(instanceResponse.statusCode);
    Serial.println(instanceResponse.errorMsg);
  }
}


void launchInstance() {
  char instancePath[20] = "/20160918/instances";
  char request[900];
  {
    DynamicJsonDocument requestJson(900);
    requestJson["availabilityDomain"] = "odti:PHX-AD-1";
    requestJson["compartmentId"] = compartmentOcid;
    requestJson["displayName"] = "cloud-deck-vm";
    requestJson["imageId"] = "ocid1.image.oc1..aaaaaaaasoykfuuflr4ks6zxxmj5astynromw3f523gcylgdonlwe4dbvaaq";  // OCI Developer Image
    requestJson["shape"] = "VM.Standard.E2.2";
    JsonObject metadata = requestJson.createNestedObject("metadata");
    metadata["ssh_authorized_keys"] = sshKey;
    JsonObject createVnicDetails = requestJson.createNestedObject("createVnicDetails");
    createVnicDetails["assignPublicIp"] = true;
    createVnicDetails["subnetId"] = "ocid1.subnet.oc1.phx.aaaaaaaapixvxoox7bwl3jfnti2jphizpaq42u7pjutyqng7zth7ur3xkuoa";
    serializeJson(requestJson, request);
  }
  OciApiRequest instanceRequest(iaasHost, instancePath, oci.HTTP_METHOD_POST, {}, 0, NULL, request);
  OciApiResponse instanceResponse;
  oci.apiCall(instanceRequest, instanceResponse);

  if( instanceResponse.statusCode == 200 ) {
    
    StaticJsonDocument<200> filter;
    filter["displayName"] = true;
    filter["id"] = true;
    filter["lifecycleState"] = true;
    
    DynamicJsonDocument doc(300);
    deserializeJson(doc, instanceResponse.response, DeserializationOption::Filter(filter));
    //Serial.println("Launch Instance Response:");
    //serializeJsonPretty(doc, Serial);  

    char message[200] = "*Launched VM:* \\n";
    strcat(message, "Display Name: ");
    strcat(message, doc["displayName"]);
    strcat(message, "\\n");
    strcat(message, "OCID: ");
    strcat(message, doc["id"]);
    strcat(message, "\\n");
    strcat(message, "State: ");
    strcat(message, doc["lifecycleState"]);
    strcat(message, "\\n");
    
    {
      sendNotification((char*) message);
    }
  }
  else {
    Serial.println(instanceResponse.statusCode);
    Serial.println(instanceResponse.errorMsg);
  }
}

void getDbInfo() {
  char dbInfoPath[200] = "/20160918/autonomousDatabases/";
  strcat(dbInfoPath, dbOcid);
  
  OciApiRequest dbInfoRequest(dbHost, dbInfoPath, oci.HTTP_METHOD_GET, {}, 0, NULL);
  OciApiResponse dbInfoResponse;
  oci.apiCall(dbInfoRequest, dbInfoResponse);

  if( dbInfoResponse.statusCode == 200 ) {
    StaticJsonDocument<200> filter;
    filter["displayName"] = true;
    filter["dbName"] = true;
    filter["id"] = true;
    filter["cpuCoreCount"] = true;
    filter["dataStorageSizeInTBs"] = true;
    filter["serviceConsoleUrl"] = true;

    DynamicJsonDocument doc(1000);
    deserializeJson(doc, dbInfoResponse.response, DeserializationOption::Filter(filter));
    //Serial.println("DB Info Response:");
    //serializeJsonPretty(doc, Serial);  
    char message[320] = "*DB Info:* \\n";
    strcat(message, "Display Name: ");
    strcat(message, doc["displayName"]);
    strcat(message, "\\n");
    strcat(message, "DB Name: ");
    strcat(message, doc["dbName"]);
    strcat(message, "\\n");
    strcat(message, "DB OCID: ");
    strcat(message, doc["id"]);
    strcat(message, "\\n");
    char cores[2];
    sprintf(cores, "%d", doc["cpuCoreCount"].as<int>());
    char storage[2];
    sprintf(storage, "%d", doc["dataStorageSizeInTBs"].as<int>());
    strcat(message, "Cores: ");
    strcat(message, cores);
    strcat(message, "\\n");
    strcat(message, "Storage Size (TB): ");
    strcat(message, storage);
    strcat(message, "\\n");
    /*
    strcat(message, "Console URL: ");
    strcat(message, doc[0]["serviceConsoleUrl"]);
    strcat(message, "\\n");
    */
    sendNotification((char*) message);
  }
  else {
    Serial.println(dbInfoResponse.statusCode);
    Serial.println(dbInfoResponse.errorMsg);
  }
}

void getRecentDbBackup() {
  char dbBackupPath[200] = "/20160918/autonomousDatabaseBackups?";
  strcat(dbBackupPath, "autonomousDatabaseId=");
  strcat(dbBackupPath, dbOcid);
  strcat(dbBackupPath, "&limit=1&sortBy=TIMECREATED");

  OciApiRequest dbBackupRequest(dbHost, dbBackupPath, oci.HTTP_METHOD_GET, {}, 0, NULL);
  OciApiResponse dbBackupResponse;
  oci.apiCall(dbBackupRequest, dbBackupResponse);

  if( dbBackupResponse.statusCode == 200 ) {
    DynamicJsonDocument doc(1200);
    deserializeJson(doc, dbBackupResponse.response);
    //Serial.println("DB Backup Response:");
    //serializeJsonPretty(doc, Serial);  
    char message[220] = "*DB Backup Info:* \\n";
    strcat(message, "DB OCID: ");
    strcat(message, doc[0]["autonomousDatabaseId"]);
    strcat(message, "\\n");
    strcat(message, "Last Backup At: ");
    strcat(message, doc[0]["displayName"]);
    strcat(message, "\\n");
    strcat(message, "Type: ");
    strcat(message, doc[0]["type"]);
    strcat(message, "\\n");

    sendNotification((char*) message);
  }
  else {
    Serial.println(dbBackupResponse.statusCode);
    Serial.println(dbBackupResponse.errorMsg);
  }
}

void getUserKeys() {
  char keyPath[200] = "/20160918/users/";
  strcat(keyPath, userOcid);
  strcat(keyPath, "/apiKeys/");
  OciApiRequest keyRequest(iamHost, keyPath, oci.HTTP_METHOD_GET, {}, 0, NULL);
  OciApiResponse keyResponse;
  oci.apiCall(keyRequest, keyResponse);
  
  if( keyResponse.statusCode == 200 ) {
    StaticJsonDocument<300> filter;
    filter[0]["timeCreated"] = true;
    filter[0]["fingerprint"] = true;
    DynamicJsonDocument keyDoc(400);
    deserializeJson(keyDoc, keyResponse.response, DeserializationOption::Filter(filter));
    //Serial.println("User Keys Response:");
    //serializeJsonPretty(keyDoc, Serial);  
    JsonArray keys = keyDoc.as<JsonArray>();

    char message[] = "*User API Keys:* \\n";
    sendNotification((char*) message);
    for (JsonVariant key : keys) {    
      char keyMsg[200] = "";
      strcat(keyMsg, "Key Fingerprint: ");
      strcat(keyMsg, key["fingerprint"].as<char*>());
      strcat(keyMsg, "\\n");
      strcat(keyMsg, "Created: ");
      strcat(keyMsg, key["timeCreated"].as<char*>());
      strcat(keyMsg, "\\n");
      sendNotification((char*) keyMsg);
    }
    
  }
  else {
    Serial.println(keyResponse.statusCode);
    Serial.println(keyResponse.errorMsg);
  }
}


void getUserInfo() {
  char userInfoPath[150] = "/20160918/users/";
  strcat(userInfoPath, userOcid);
  
  OciApiRequest userInfoRequest(iamHost, userInfoPath, oci.HTTP_METHOD_GET, {}, 0, NULL);
  OciApiResponse userInfoResponse;
  oci.apiCall(userInfoRequest, userInfoResponse);

  if( userInfoResponse.statusCode == 200 ) {
    DynamicJsonDocument doc(800);
    deserializeJson(doc, userInfoResponse.response);

    char message[520] = "*User Info:* \\n";
    strcat(message, "Username: ");
    strcat(message, doc["name"]);
    strcat(message, "\\n");
    strcat(message, "Name: ");
    strcat(message, doc["description"]);
    strcat(message, "\\n");
    strcat(message, "OCID: ");
    strcat(message, doc["id"]);
    strcat(message, "\\n");
    strcat(message, "Using MFA: ");
    const char* mfa = (doc["isMfaActivated"] ? "Yes" : "No");
    strcat(message, mfa);
    strcat(message, "\\n");
      
    char keyPath[200] = "/20160918/users/";
    strcat(keyPath, doc["id"]);
    strcat(keyPath, "/apiKeys/");
    OciApiRequest keyRequest(iamHost, keyPath, oci.HTTP_METHOD_GET, {}, 0, NULL);
    OciApiResponse keyResponse;
    oci.apiCall(keyRequest, keyResponse);
    //Serial.println("User Info Response:");
    //serializeJsonPretty(doc, Serial);  

    sendNotification((char*) message);
  }
  else {
    Serial.println(userInfoResponse.statusCode);
    Serial.println(userInfoResponse.errorMsg);
  }
}

void getTenancyInfo() {
  char tenancyInfoPath[150] = "/20160918/tenancies/";
  strcat(tenancyInfoPath, tenancyOcid);
  
  OciApiRequest tenancyInfoRequest(iamHost, tenancyInfoPath, oci.HTTP_METHOD_GET, {}, 0, NULL);
  Header resHeaders[] = { {"opc-request-id"} };
  OciApiResponse tenancyInfoResponse(resHeaders, 1);
  oci.apiCall(tenancyInfoRequest, tenancyInfoResponse);
  if( tenancyInfoResponse.statusCode == 200 ) {
    DynamicJsonDocument doc(800);
    deserializeJson(doc, tenancyInfoResponse.response);
    //Serial.println("Tenancy Info Response:");
    //serializeJsonPretty(doc, Serial);  
    //Serial.println("Response Header: ");
    //Serial.println(tenancyInfoResponse.responseHeaders[0].headerValue);
    char message[200] = "*Tenancy Info:* \\n";
    strcat(message, "Name: ");
    strcat(message, doc["name"]);
    strcat(message, "\\n");
    strcat(message, "OCID: ");
    strcat(message, doc["id"]);
    strcat(message, "\\n");
    strcat(message, "Home Region: ");
    strcat(message, doc["homeRegion"]);
    strcat(message, "\\n");
    sendNotification((char*) message);
  }
  else {
    Serial.println(tenancyInfoResponse.statusCode);
    Serial.println(tenancyInfoResponse.errorMsg);
  }
}

void sendNotification(char* notification) {
  char notificationPath[150] = "/20181201/topics/";
  strcat(notificationPath, notificationTopicOcid);
  strcat(notificationPath, "/messages");
  char message[350] = "{ \"title\": \"Cloud Deck Notification\", \"body\": \"";
  strcat(message, notification);
  strcat(message, "\" }");
  OciApiRequest sendNotificationRequest(notificationHost, notificationPath, oci.HTTP_METHOD_POST, {}, 0, NULL, message);
  OciApiResponse sendNotificationResponse;
  oci.apiCall(sendNotificationRequest, sendNotificationResponse);

  if( sendNotificationResponse.statusCode == 202 ) {
    DynamicJsonDocument doc(500);
    deserializeJson(doc, sendNotificationResponse.response);
    //Serial.println("Send Notification Response:");
    //serializeJsonPretty(doc, Serial);  
  }
  else {
    Serial.println(sendNotificationResponse.errorMsg);
  }
}

// Button helper class for handling press/release and debouncing
class button {
  public:
  const uint8_t pin;

  button(uint8_t p) : pin(p){}

  void press(boolean state){
    if(state == pressed || (millis() - lastPressed  <= debounceTime)){
      return; // Nothing to see here, folks
    }

    lastPressed = millis();
    pressed = state;

    if(state == 1) {
      switch( pin ) {
        case 13:
          // key 1
          Serial.println(F("Getting Tenancy Information..."));
          getTenancyInfo();
          Serial.println(F("Tenancy Information Sent..."));
          break;
        case 12:
          // key 2
          Serial.println(F("Getting User Information..."));
          getUserInfo();
          Serial.println(F("User Information Sent..."));
          break;
        case 14:
          // key 3
          Serial.println(F("Getting User API Keys..."));
          getUserKeys();
          Serial.println(F("User API Keys Sent..."));
          break;
        case 27:
          // key 4
          Serial.println(F("Getting DB Info..."));
          getDbInfo();
          Serial.println(F("DB Info Sent..."));
          break;
        case 26:
          // key 5
          Serial.println(F("Getting DB Backup Information..."));
          getRecentDbBackup();
          Serial.println(F("DB Backup Information Sent..."));
          break;
        case 25:
          // key 6
          Serial.println(F("Launching VM..."));
          launchInstance();
          Serial.println(F("VM Launch Confirmation Sent..."));
          break;
        case 33:
          // key 7
          Serial.println(F("Starting QA Env..."));
          toggleQaDb(true);
          toggleQaVm(true);
          Serial.println(F("QA Env Start Notification Sent..."));
          break;
        case 32:
          // key 8
          Serial.println(F("Stopping QA Env..."));
          toggleQaDb(false);
          toggleQaVm(false);
          Serial.println(F("QA Env Stop Notification Sent..."));
          break;
      }
    }
  }

  void update(){
    press(!digitalRead(pin));
  }

  private:
  const unsigned long debounceTime = 1000;
  unsigned long lastPressed = 0;
  boolean pressed = 0;
};

// Button objects, organized in array
button buttons[] = {
  {BUTTON_PIN1},
  {BUTTON_PIN2},
  {BUTTON_PIN3},
  {BUTTON_PIN4},
  {BUTTON_PIN5},
  {BUTTON_PIN6},
  {BUTTON_PIN7},
  {BUTTON_PIN8}
};

const uint8_t numButtons = sizeof(buttons) / sizeof(button);
const uint8_t ledPin = 2;

void setup() { 
  Serial.begin(115200);
  Serial.println("Connecting to " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.println(F("."));
  }
  Serial.print("Connected.\n\n");

  // Safety check. Ground pin #1 (RX) to cancel keyboard inputs.
  pinMode(1, INPUT_PULLUP);
  if(!digitalRead(1)){
    failsafe();
  }

  // Set LEDs Off. Active low.
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  for(int i = 0; i < numButtons; i++){
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
}

void loop() {
  for(int i = 0; i < numButtons; i++){
    buttons[i].update();
  }
}

void failsafe(){
  for(;;){} // Just going to hang out here for awhile :D
}
