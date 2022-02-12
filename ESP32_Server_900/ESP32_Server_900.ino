#include <FS.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define USBCONTROL false // set to true if you are using usb control
#define usbPin 4  // set the pin you want to use for usb control


                    // enable internal goldhen.h [ true / false ]
#define INTHEN true // goldhen is placed in the app partition to free up space on the storage for other payloads.
                    // with this enabled you do not upload goldhen to the board, set this to false if you wish to upload goldhen.

                      // enable autohen [ true / false ]
#define AUTOHEN false // this will load goldhen instead of the normal index/payload selection page, use this if you only want hen and no other payloads.
                      // INTHEN must be set to true for this to work.

#include "Pages.h"
#include "Loader.h"

#if INTHEN
#include "goldhen.h"
#endif

//-------------------DEFAULT SETTINGS------------------//

//create access point
boolean startAP = true;
String AP_SSID = "PS4_WEB_AP";
String AP_PASS = "password";
IPAddress Server_IP(10,1,1,1);
IPAddress Subnet_Mask(255,255,255,0);

//connect to wifi
boolean connectWifi = false;
String WIFI_SSID = "Home_WIFI";
String WIFI_PASS = "password";
String WIFI_HOSTNAME = "ps4.local";

//server port
int WEB_PORT = 80;

//Auto Usb Wait(milliseconds)
int USB_WAIT = 10000;
//-----------------------------------------------------//


String firmwareVer = "1.00";
DNSServer dnsServer;
AsyncWebServer server(WEB_PORT);
boolean hasEnabled = false;
long enTime = 0;
File upFile;


String split(String str, String from, String to)
{
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());   
  String retval = str.substring(pos1 + from.length() , pos2);
  return retval;
}


bool instr(String str, String search)
{
int result = str.indexOf(search);
if (result == -1)
{
  return false;
}
return true;
}


String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+" B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+" KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+" MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+" GB";
  }
}


String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
      }
      yield();
    }
    encodedString.replace("%2E",".");
    return encodedString;
}


String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".bin")) return "application/octet-stream";
  else if(filename.endsWith(".manifest")) return "text/cache-manifest";
  return "text/plain";
}


void disableUSB()
{
   enTime = 0;
   hasEnabled = false;
   digitalWrite(usbPin, LOW);
}

void enableUSB()
{
   digitalWrite(usbPin, HIGH);
   enTime = millis();
   hasEnabled = true;
}


void sendwebmsg(AsyncWebServerRequest *request, String htmMsg)
{
    String tmphtm = "<!DOCTYPE html><html><head><style>body { background-color: #1451AE;color: #ffffff;font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>" + htmMsg + "</center></html>";
    request->send(200, "text/html", tmphtm);
}


void handleFwUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index){
      String path = request->url();
      if (path != "/update.html") {
        request->send(500, "text/plain", "Internal Server Error");
        return;
      }
      if (!filename.equals("fwupdate.bin")) {
        sendwebmsg(request, "Invalid update file: " + filename);
        return;
      }
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      //Serial.printf("Update Start: %s\n", filename.c_str());
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
        Update.printError(Serial);
        sendwebmsg(request, "Update Failed: " + String(Update.errorString()));
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
        sendwebmsg(request,  "Update Failed: " + String(Update.errorString()));
      }
    }
    if(final){
      if(Update.end(true)){
        //Serial.printf("Update Success: %uB\n", index+len);
        String tmphtm = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style>body { background-color: #1451AE;color: #ffffff;font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>Update Success, Rebooting.</center></html>";
        request->send(200, "text/html", tmphtm);
        delay(1000);
        ESP.restart();
      } else {
        Update.printError(Serial);
      }
    }
}


void handleDelete(AsyncWebServerRequest *request){
  if(!request->hasParam("file", true))
  {
    request->redirect("/fileman.html"); 
    return;
  }
  String path = request->getParam("file", true)->value();
  if(path.length() == 0) 
  {
    request->redirect("/fileman.html"); 
    return;
  }
  if (SPIFFS.exists("/" + path) && path != "/" && !path.equals("config.ini")) {
    SPIFFS.remove("/" + path);
  }
  request->redirect("/fileman.html"); 
}



void handleFileMan(AsyncWebServerRequest *request) {
  File dir = SPIFFS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Manager</title><link rel=\"stylesheet\" href=\"style.css\"><style>body{overflow-y:auto;}</style><script>function statusDel(fname) {var answer = confirm(\"Are you sure you want to delete \" + fname + \" ?\");if (answer) {return true;} else { return false; }}</script></head><body><br><table>"; 
  int fileCount = 0;
  File file = dir.openNextFile();
  while(file){
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini"))
    {
    fileCount++;
    output += "<tr>";
    output += "<td><a href=\"" +  fname + "\">" + fname + "</a></td>";
    output += "<td>" + formatBytes(file.size()) + "</td>";
    output += "<td><a href=\"/" + fname + "\" download><button type=\"submit\">Download</button></a></td>";
    output += "<td><form action=\"/delete\" method=\"post\"><button type=\"submit\" name=\"file\" value=\"" + fname + "\" onClick=\"return statusDel('" + fname + "');\">Delete</button></form></td>";
    output += "</tr>";
    }
    file.close();
    file = dir.openNextFile();
  }
  if (fileCount == 0)
  {
      output += "<p><center>No files found<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p>";
  }
  output += "</table></body></html>";
  request->send(200, "text/html", output);
}


void handlePayloads(AsyncWebServerRequest *request) {
  File dir = SPIFFS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>ESP Server</title><link rel=\"stylesheet\" href=\"style.css\"><style>body { background-color: #1451AE; color: #ffffff; font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; overflow-y:hidden; text-shadow: 3px 2px DodgerBlue;}</style><script>function setpayload(payload,title,waittime){ sessionStorage.setItem('payload', payload); sessionStorage.setItem('title', title); sessionStorage.setItem('waittime', waittime);  window.open('loader.html', '_self');}</script></head><body><center><h1>9.00 Payloads</h1>";
  int cntr = 0;
  int payloadCount = 0;
  if (USB_WAIT < 5000){USB_WAIT = 5000;} // correct unrealistic timing values
  if (USB_WAIT > 25000){USB_WAIT = 25000;}

#if INTHEN
  payloadCount++;
  cntr++;
  output +=  "<a onclick=\"setpayload('gldhen.bin','" + String(INTHEN_NAME) + "','" + String(USB_WAIT) + "')\"><button class=\"btn\">" + String(INTHEN_NAME) + "</button></a>&nbsp;";
#endif

  File file = dir.openNextFile();
  while(file){
    String fname = String(file.name());
    if (fname.endsWith(".gz")) {
        fname = fname.substring(0, fname.length() - 3);
    }
    if (fname.length() > 0 && fname.endsWith(".bin"))
    {
      payloadCount++;
      String fnamev = fname;
      fnamev.replace(".bin","");
      output +=  "<a onclick=\"setpayload('" + urlencode(fname) + "','" + fnamev + "','" + String(USB_WAIT) + "')\"><button class=\"btn\">" + fnamev + "</button></a>&nbsp;";
      cntr++;
      if (cntr == 3)
      {
        cntr = 0;
        output +=  "<p></p>";
      }
    }
    file.close();
    file = dir.openNextFile();
  }
  if (payloadCount == 0)
  {
      output += "<msg>No .bin payloads found<br>You need to upload the payloads to the ESP32 board.<br>in the arduino ide select <b>Tools</b> &gt; <b>ESP32 Sketch Data Upload</b><br>or<br>Using a pc/laptop connect to <b>" + AP_SSID + "</b> and navigate to <a href=\"/admin.html\"><u>http://" + WIFI_HOSTNAME + "/admin.html</u></a> and upload the .bin payloads using the <b>File Uploader</b></msg></center></body></html>";
  }
  output += "</center></body></html>";
  request->send(200, "text/html", output);
}


void handleConfig(AsyncWebServerRequest *request)
{
  if(request->hasParam("ap_ssid", true) && request->hasParam("ap_pass", true) && request->hasParam("web_ip", true) && request->hasParam("web_port", true) && request->hasParam("subnet", true) && request->hasParam("wifi_ssid", true) && request->hasParam("wifi_pass", true) && request->hasParam("wifi_host", true) && request->hasParam("usbwait", true)) 
  {
    AP_SSID = request->getParam("ap_ssid", true)->value();
    if (!request->getParam("ap_pass", true)->value().equals("********"))
    {
      AP_PASS = request->getParam("ap_pass", true)->value();
    }
    WIFI_SSID = request->getParam("wifi_ssid", true)->value();
    if (!request->getParam("wifi_pass", true)->value().equals("********"))
    {
      WIFI_PASS = request->getParam("wifi_pass", true)->value();
    }
    String tmpip = request->getParam("web_ip", true)->value();
    String tmpwport = request->getParam("web_port", true)->value();
    String tmpsubn = request->getParam("subnet", true)->value();
    String WIFI_HOSTNAME = request->getParam("wifi_host", true)->value();
    String tmpua = "false";
    String tmpcw = "false";
    if (request->hasParam("useap", true)){tmpua = "true";}
    if (request->hasParam("usewifi", true)){tmpcw = "true";}
    int USB_WAIT = request->getParam("usbwait", true)->value().toInt();
    File iniFile = SPIFFS.open("/config.ini", "w");
    if (iniFile) {
    iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + tmpip + "\r\nWEBSERVER_PORT=" + tmpwport + "\r\nSUBNET_MASK=" + tmpsubn + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + USB_WAIT + "\r\n");
    iniFile.close();
    }
    String htmStr = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {z-index: 1;width: 50px;height: 50px;margin: 0 0 0 0;border: 6px solid #f3f3f3;border-radius: 50%;border-top: 6px solid #3498db;width: 50px;height: 50px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite; } @-webkit-keyframes spin {0%{-webkit-transform: rotate(0deg);}100%{-webkit-transform: rotate(360deg);}}@keyframes spin{0%{ transform: rotate(0deg);}100%{transform: rotate(360deg);}}body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;} #msgfmt {font-size: 16px; font-weight: normal;}#status {font-size: 16px; font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Config saved<br>Rebooting</p></center></html>";
    request->send(200, "text/html", htmStr);
    delay(1000);
    ESP.restart();
  }
  else
  {
   request->redirect("/config.html");
  }
}


void handleReboot(AsyncWebServerRequest *request)
{
  //HWSerial.print("Rebooting ESP");
  request->send(200, "text/html", rebootingData);
  delay(1000);
  ESP.restart();
}



void handleConfigHtml(AsyncWebServerRequest *request)
{
  String tmpUa = "";
  String tmpCw = "";
  if (startAP){tmpUa = "checked";}
  if (connectWifi){tmpCw = "checked";}
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Config Editor</title><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 14px;font-weight: bold;margin: 0 0 0 0.0;padding: 0.4em 0.4em 0.4em 0.6em;}input[type=\"submit\"]:hover {background: #ffffff;color: green;}input[type=\"submit\"]:active{outline-color: green;color: green;background: #ffffff; }table {font-family: arial, sans-serif;border-collapse: collapse;}td {border: 1px solid #dddddd;text-align: left;padding: 8px;}th {border: 1px solid #dddddd; background-color:gray;text-align: center;padding: 8px;}</style></head><body><form action=\"/config.html\" method=\"post\"><center><table><tr><th colspan=\"2\"><center>Access Point</center></th></tr><tr><td>AP SSID:</td><td><input name=\"ap_ssid\" value=\"" + AP_SSID + "\"></td></tr><tr><td>AP PASSWORD:</td><td><input name=\"ap_pass\" value=\"********\"></td></tr><tr><td>AP IP:</td><td><input name=\"web_ip\" value=\"" + Server_IP.toString() + "\"></td></tr><tr><td>SUBNET MASK:</td><td><input name=\"subnet\" value=\"" + Subnet_Mask.toString() + "\"></td></tr><tr><td>START AP:</td><td><input type=\"checkbox\" name=\"useap\" " + tmpUa +"></td></tr><tr><th colspan=\"2\"><center>Web Server</center></th></tr><tr><td>WEBSERVER PORT:</td><td><input name=\"web_port\" value=\"" + String(WEB_PORT) + "\"></td></tr><tr><th colspan=\"2\"><center>Wifi Connection</center></th></tr><tr><td>WIFI SSID:</td><td><input name=\"wifi_ssid\" value=\"" + WIFI_SSID + "\"></td></tr><tr><td>WIFI PASSWORD:</td><td><input name=\"wifi_pass\" value=\"********\"></td></tr><tr><td>WIFI HOSTNAME:</td><td><input name=\"wifi_host\" value=\"" + WIFI_HOSTNAME + "\"></td></tr><tr><td>CONNECT WIFI:</td><td><input type=\"checkbox\" name=\"usewifi\" " + tmpCw + "></tr><tr><th colspan=\"2\"><center>Auto USB Wait</center></th></tr><tr><td>WAIT TIME(ms):</td><td><input name=\"usbwait\" value=\"" + USB_WAIT + "\"></td></tr></table><br><input id=\"savecfg\" type=\"submit\" value=\"Save Config\"></center></form></body></html>";
  request->send(200, "text/html", htmStr);
}



void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index){
      String path = request->url();
      if (path != "/upload.html") {
        request->send(500, "text/plain", "Internal Server Error");
        return;
      }
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      if (filename.equals("/config.ini"))
      {return;}
      //Serial.printf("Upload Start: %s\n", filename.c_str());
      upFile = SPIFFS.open(filename, "w");
      }
    if(upFile){
        upFile.write(data, len);
    }
    if(final){
        upFile.close();
        //Serial.printf("upload Success: %uB\n", index+len);
    }
}


void handleConsoleUpdate(String rgn, AsyncWebServerRequest *request)
{
  String Version = "05.050.000";
  String sVersion = "05.050.000";
  String lblVersion = "5.05";
  String imgSize = "0";
  String imgPath = "";
  String xmlStr = "<?xml version=\"1.0\" ?><update_data_list><region id=\"" + rgn + "\"><force_update><system level0_system_ex_version=\"0\" level0_system_version=\"" + Version + "\" level1_system_ex_version=\"0\" level1_system_version=\"" + Version + "\"/></force_update><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"><update_data update_type=\"full\"><image size=\"" + imgSize + "\">" + imgPath + "</image></update_data></system_pup><recovery_pup type=\"default\"><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"/><image size=\"" + imgSize + "\">" + imgPath + "</image></recovery_pup></region></update_data_list>";
  request->send(200, "text/xml", xmlStr);
}


void handleInfo(AsyncWebServerRequest *request)
{
  float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
  FlashMode_t ideMode = ESP.getFlashChipMode();
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>System Information</title><style type=\"text/css\">body { background-color: #1451AE;color: #ffffff;font-size: 14px;font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head>";
  output += "<hr>###### Software ######<br><br>";
  output += "Firmware version " + firmwareVer + "<br>";
  output += "SDK version: " + String(ESP.getSdkVersion()) + "<br>";
  output += "Chip Id: " + String(ESP.getChipModel()) + "<br><hr>";
  output += "###### CPU ######<br><br>";
  output += "CPU frequency: " + String(ESP.getCpuFreqMHz()) + "MHz<br>";
  output += "Cores: " + String(ESP.getChipCores()) + "<br><hr>";
  output += "###### Flash chip information ######<br><br>";
  output += "Flash chip Id: " +  String(ESP.getFlashChipMode()) + "<br>";
  output += "Estimated Flash size: " + formatBytes(ESP.getFlashChipSize()) + "<br>";
  output += "Flash frequency: " + String(flashFreq) + " MHz<br>";
  output += "Flash write mode: " + String((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN")) + "<br><hr>";
  output += "###### Storage information ######<br><br>";
  output += "Filesystem: SPIFFS<br>";
  output += "Total Size: " + formatBytes(SPIFFS.totalBytes()) + "<br>";
  output += "Used Space: " + formatBytes(SPIFFS.usedBytes()) + "<br>";
  output += "Free Space: " + formatBytes(SPIFFS.totalBytes() - SPIFFS.usedBytes()) + "<br><hr>";
  output += "###### Ram information ######<br><br>";
  output += "Ram size: " + formatBytes(ESP.getHeapSize()) + "<br>";
  output += "Free ram: " + formatBytes(ESP.getFreeHeap()) + "<br>";
  output += "Max alloc ram: " + formatBytes(ESP.getMaxAllocHeap()) + "<br><hr>";
  output += "###### Sketch information ######<br><br>";
  output += "Sketch hash: " + ESP.getSketchMD5() + "<br>";
  output += "Sketch size: " +  formatBytes(ESP.getSketchSize()) + "<br>";
  output += "Free space available: " +  formatBytes(ESP.getFreeSketchSpace() - ESP.getSketchSize()) + "<br><hr>";
  output += "</html>";
  request->send(200, "text/html", output);
}


void handleCacheManifest(AsyncWebServerRequest *request) {
  #if !USBCONTROL
  String output = "CACHE MANIFEST\r\n";
  File dir = SPIFFS.open("/");
  File file = dir.openNextFile();
  while(file){
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini"))
    {
      if (fname.endsWith(".gz")) {
        fname = fname.substring(0, fname.length() - 3);
      }
     output += urlencode(fname) + "\r\n";
    }
     file.close();
     file = dir.openNextFile();
  }
  if(!instr(output,"index.html\r\n"))
  {
    output += "index.html\r\n";
  }
  if(!instr(output,"menu.html\r\n"))
  {
    output += "menu.html\r\n";
  }
  if(!instr(output,"loader.html\r\n"))
  {
    output += "loader.html\r\n";
  }
  if(!instr(output,"payloads.html\r\n"))
  {
    output += "payloads.html\r\n";
  }
  if(!instr(output,"style.css\r\n"))
  {
    output += "style.css\r\n";
  }
#if INTHEN
  output += "gldhen.bin\r\n";
#endif
   request->send(200, "text/cache-manifest", output);
  #else
   request->send(404);
  #endif
}


void writeConfig()
{
  File iniFile = SPIFFS.open("/config.ini", "w");
  if (iniFile) {
  String tmpua = "false";
  String tmpcw = "false";
  if (startAP){tmpua = "true";}
  if (connectWifi){tmpcw = "true";}
  iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nWEBSERVER_PORT=" + String(WEB_PORT) + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + USB_WAIT + "\r\n");
  iniFile.close();
  }
}


void setup(){
  pinMode(usbPin, OUTPUT); 
  digitalWrite(usbPin, LOW);
  //Serial.begin(115200);
  //Serial.println("Version: " + firmwareVer);
  if (SPIFFS.begin(true)) {
  if (SPIFFS.exists("/config.ini")) {
  File iniFile = SPIFFS.open("/config.ini", "r");
  if (iniFile) {
  String iniData;
    while (iniFile.available()) {
      char chnk = iniFile.read();
      iniData += chnk;
    }
   iniFile.close();
   
   if(instr(iniData,"SSID="))
   {
   AP_SSID = split(iniData,"SSID=","\r\n");
   AP_SSID.trim();
   }
   
   if(instr(iniData,"PASSWORD="))
   {
   AP_PASS = split(iniData,"PASSWORD=","\r\n");
   AP_PASS.trim();
   }
   
   if(instr(iniData,"WEBSERVER_IP="))
   {
    String strwIp = split(iniData,"WEBSERVER_IP=","\r\n");
    strwIp.trim();
    Server_IP.fromString(strwIp);
   }

   if(instr(iniData,"SUBNET_MASK="))
   {
    String strsIp = split(iniData,"SUBNET_MASK=","\r\n");
    strsIp.trim();
    Subnet_Mask.fromString(strsIp);
   }
   
   if(instr(iniData,"WIFI_SSID="))
   {
   WIFI_SSID = split(iniData,"WIFI_SSID=","\r\n");
   WIFI_SSID.trim();
   }
   
   if(instr(iniData,"WIFI_PASS="))
   {
   WIFI_PASS = split(iniData,"WIFI_PASS=","\r\n");
   WIFI_PASS.trim();
   }

   if(instr(iniData,"WIFI_HOST="))
   {
   WIFI_HOSTNAME = split(iniData,"WIFI_HOST=","\r\n");
   WIFI_HOSTNAME.trim();
   }

   if(instr(iniData,"USEAP="))
   {
    String strua = split(iniData,"USEAP=","\r\n");
    strua.trim();
    if (strua.equals("true"))
    {
      startAP = true;
    }
    else
    {
      startAP = false;
    }
   }

   if(instr(iniData,"CONWIFI="))
   {
    String strcw = split(iniData,"CONWIFI=","\r\n");
    strcw.trim();
    if (strcw.equals("true"))
    {
      connectWifi = true;
    }
    else
    {
      connectWifi = false;
    }
   }
   if(instr(iniData,"USBWAIT="))
   {
    String strusw = split(iniData,"USBWAIT=","\r\n");
    strusw.trim();
    USB_WAIT = strusw.toInt();
   }
   }
  }
  else
  {
   writeConfig(); 
  }
  }
  else
  {
    //HWSerial.println("SPIFFS failed to mount");
  }


  if (startAP)
  {
    //HWSerial.println("SSID: " + AP_SSID);
    //HWSerial.println("Password: " + AP_PASS);
    //HWSerial.println("");
    //HWSerial.println("WEB Server IP: " + Server_IP.toString());
    //HWSerial.println("Subnet: " + Subnet_Mask.toString());
    //HWSerial.println("WEB Server Port: " + String(WEB_PORT));
    //HWSerial.println("");
    WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
    //HWSerial.println("WIFI AP started");
    dnsServer.setTTL(30);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", Server_IP);
    //HWSerial.println("DNS server started");
    //HWSerial.println("DNS Server IP: " + Server_IP.toString());
  }

  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0)
  {
    WiFi.setAutoConnect(true); 
    WiFi.setAutoReconnect(true);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    //HWSerial.println("WIFI connecting");
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      //HWSerial.println("Wifi failed to connect");
    } else {
      IPAddress LAN_IP = WiFi.localIP(); 
      if (LAN_IP)
      {
        //HWSerial.println("Wifi Connected");
        //HWSerial.println("WEB Server LAN IP: " + LAN_IP.toString());
        //HWSerial.println("WEB Server Port: " + String(WEB_PORT));
        //HWSerial.println("WEB Server Hostname: " + WIFI_HOSTNAME);
        String mdnsHost = WIFI_HOSTNAME;
        mdnsHost.replace(".local","");
        MDNS.begin(mdnsHost.c_str());
        if (!startAP)
        {
          dnsServer.setTTL(30);
          dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
          dnsServer.start(53, "*", LAN_IP);
          //HWSerial.println("DNS server started");
          //HWSerial.println("DNS Server IP: " + LAN_IP.toString());
        }
      }
    }
  }


  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
   request->send(200, "text/plain", "Microsoft Connect Test");
  });

  server.on("/cache.manifest", HTTP_GET, [](AsyncWebServerRequest *request){
   handleCacheManifest(request);
  });

  server.on("/upload.html", HTTP_GET, [](AsyncWebServerRequest *request){
   request->send(200, "text/html", uploadData);
  });

   server.on("/upload.html", HTTP_POST, [](AsyncWebServerRequest *request){
     request->redirect("/fileman.html"); 
  }, handleFileUpload);

  server.on("/fileman.html", HTTP_GET, [](AsyncWebServerRequest *request){
   handleFileMan(request);
  });

  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request){
   handleDelete(request);
  });

  server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request){
   handleConfigHtml(request);
  });
  
  server.on("/config.html", HTTP_POST, [](AsyncWebServerRequest *request){
   handleConfig(request);
  });
  
  server.on("/admin.html", HTTP_GET, [](AsyncWebServerRequest *request){
   request->send(200, "text/html", adminData);
  });
  
  server.on("/reboot.html", HTTP_GET, [](AsyncWebServerRequest *request){
   request->send(200, "text/html", rebootData);
  });
  
  server.on("/reboot.html", HTTP_POST, [](AsyncWebServerRequest *request){
   handleReboot(request);
  });

  server.on("/update.html", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(200, "text/html", updateData);
  });

   server.on("/update.html", HTTP_POST, [](AsyncWebServerRequest *request){
  }, handleFwUpdate);

  server.on("/info.html", HTTP_GET, [](AsyncWebServerRequest *request){
     handleInfo(request);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
   request->send(200, "text/css", styleData);
  });

  server.on("/usbon", HTTP_POST, [](AsyncWebServerRequest *request){
     enableUSB();
     request->send(200, "text/plain", "ok");
  });

  server.on("/usboff", HTTP_POST, [](AsyncWebServerRequest *request){
     disableUSB();
     request->send(200, "text/plain", "ok");
  });

#if INTHEN
  server.on("/gldhen.bin", HTTP_GET, [](AsyncWebServerRequest *request){
   AsyncWebServerResponse *response = request->beginResponse_P(200, "application/octet-stream", goldhen_gz, sizeof(goldhen_gz));
   response->addHeader("Content-Encoding", "gzip");
   request->send(response);
  });
#endif

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request){
     //Serial.println(request->url());
     String path = request->url();
     if (instr(path,"/update/ps4/"))
     {
        String Region = split(path,"/update/ps4/list/","/");
        handleConsoleUpdate(Region, request);
        return;
     }
     if (instr(path,"/document/") && instr(path,"/ps4/"))
     {
        request->redirect("http://" + WIFI_HOSTNAME + "/index.html");
        return;
     }
     if (path.endsWith("index.html") || path.endsWith("index.htm") || path.endsWith("/"))
     {
        request->send(200, "text/html", indexData);
        return;
     }
     if (path.endsWith("menu.html"))
     {
        request->send(200, "text/html", menuData);
        return;
     }
     if (path.endsWith("payloads.html"))
     {
        #if INTHEN && AUTOHEN
          request->send(200, "text/html", autohenData);
        #else
          handlePayloads(request);
        #endif
        return;
     }
     if (path.endsWith("loader.html"))
     {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", loader_gz, sizeof(loader_gz));
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        return;
     }
    request->send(404);
  });


  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  //Serial.println("HTTP server started");
}
 
void loop(){
   if (hasEnabled && millis() >= (enTime + 15000))
   {
    disableUSB();
   } 
   dnsServer.processNextRequest();
}
