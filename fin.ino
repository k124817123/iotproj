//include lib
#include <Stepper.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
//init
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
Stepper myStepper(200,15,16,17,18);
SoftwareSerial ESP8266(10, 9); // RX, TX
//def
const String SSID="CSIE-WLAN";//Wifi名稱
const String PASSWORD="wificsie";//Wifi密碼
const int sensorPin = A0;
const int trig = 7;
const int echo = 6;
bool wifiReady = false;
bool linkok = false;
bool f2m = false;
String lcdwd="";
int Bump=0;
unsigned long Time;
float Sampling_Time;
int Num_Samples=300;
int Peak_Threshold_Factor = 85;
int Minimum_Peak_Separation = 50;  // 50*5=250 ms
int Moving_Average_Num = 20;
int Index1, Index2,k,  i, j, count,Pulse_Rate;
float  Peak1, Peak2,  PR1,  ADC_Range,Sum_Points,Num_Points;
float Peak_Magnitude, Peak_Threshold, Minima, Range;            
int data[310]; 
void setup() {
  // put your setup code here, to run once:
  lcd.begin(16, 2);                   //lcd
  myStepper.setSpeed(120);             //motor
  
  Serial.begin(9600);
  ESP8266.begin(9600);
  
  pinMode (trig, OUTPUT);
  pinMode (echo, INPUT);

  myStepper.step(200);

}


bool waitForResponse(int timeout,String keyword1,String keyword2=""){
  bool found = false;
  while(true){
    String response = readLine();
    found = findKeyword(response,keyword1) || findKeyword(response,keyword2);
    if(found || timeout<=0){//找到或時間到則跳出
      // ESP8266.flush();
      break;
    }else{//再等一下
      delay(10);
      timeout-=10;
      if(response!=""){//雖然沒找到關鍵字，但至少有一行回應，給予加碼時間。
        timeout+=100;
      }
    }
  }
  return found;
}
void stepper() {
  // step one revolution  in one direction:
  myStepper.step(200);
  //delay(500);
}
void hcsr() {
  float duration, distance;
  digitalWrite(trig, HIGH);
  delayMicroseconds(100);
  digitalWrite(trig, LOW);
  duration = pulseIn (echo, HIGH);
  distance = (duration/2)/29;
  if(distance<7){
    Bump=1;    
    lcdwd="bump";
  }
  else{
    Bump=0;
    lcdwd="    ";
    }
  lcd.setCursor(0,0);
  lcd.print("          ");
  lcd.setCursor(0, 0);
  lcd.print(lcdwd);
  //delay(inter_time);
}
void senseHeartRate()
{
    Time = millis();
    readdata();
    Sampling_Time=( millis()-Time)*1000/count; 
    RemoveDC();
    ScaleData();
    //FilterData();  
    ComputeHeartRate();
    count=0;
   // Serial.println(Pulse_Rate);
    lcd.setCursor(0, 1);
    lcd.print("   ");
    lcd.setCursor(0, 1);
    lcd.print(Pulse_Rate);
    //BTSerial.println(count*6);          //use bluetooth to send result to android
}
void senddata(String s){
    //log("send");
    ESP8266.println("AT+CIPSEND="+String(s.length()));
    log(s+" "+s.length()); 
    ESP8266.println(s);
    while(!waitForResponse(1000,"SEND OK\r\n"));
   // log("SEND..");
    log("SEND OK");
//    String line=readLine();
//    log(line);
  }
 void dspeed(){
        lcd.setCursor(0, 0);
        lcd.print("heart error");
        myStepper.setSpeed(120);
        myStepper.setSpeed(90);
        myStepper.setSpeed(60);
        myStepper.setSpeed(30);
        myStepper.setSpeed(0);     
   }
//等候特定的回應出現
//timeout 逾時時間，超過此時間，則會停止等待。
//keyword1,keyword2 在回應中要等候出現的關鍵字。
//如果關鍵字出現，就傳回 true，如果逾時都沒等到，就傳回 false。

//連線到 Wifi，並將 ESP8266設置為伺服器模式。
//成功則傳回 true，否則傳回 false。
bool connectToWifi(){
  wifiReady=false;
  log("Contacting ESP8266...");
  sendCmd("AT");
  // 成功連線到 ESP8266
  if(waitForResponse(1000,"OK\r\n")){
    log("Got response from ESP8266.");
    sendCmd("AT+CWMODE=1");
    log("Connecting to wifi...");
    sendCmd("AT+CWJAP=\""+SSID+"\",\""+PASSWORD+"\""); //注意，這裡使用了斜線「\」作為跳脫字元，以便在引號當中使用引號符號。
    if(waitForResponse(6000,"OK\r\n")){
    log("Wifi connected.");
    
 wifiReady=true;   
    }else{
      log("ERROR: Failed to connect to Wifi.");
    }
  }else{//沒有成功連線：無法取得 OK回應。
    log("ERROR: Failed to contact ESP8266.");
  }
  return wifiReady;
}
//傳送命令並等候 ESP8266的回應
//cmdString 命令
void sendCmd(String cmdString){
  cmdString=cmdString+"\r\n";
  ESP8266.print(cmdString);
  ESP8266.flush();
}
//加入記錄
void log(String msg){
    Serial.println(msg);
}
//指出字串中是否包含指定的關鍵字
//常用關鍵字："OK\r\n","ERROR\r\n","no change\r\n","no this fun\r\n"
bool findKeyword(String text,String keyword){
  if(keyword==""){
    return false;
  }
  return text.lastIndexOf(keyword)>-1;
}
//讀取一行文字
String readLine(){
  String lineString="";
  while(ESP8266.available()){
    char inChar = ESP8266.read();
    lineString=lineString+String(inChar);
    if(inChar=='\n'){
      break;
    }    
  }
  return lineString;
}
void proxyToESP8266(){
  while (Serial.available()) {
    int inByte = Serial.read();
    ESP8266.write(inByte);
  }
}
void readdata()
{
    while(1) {       // sense 10 seconds
        data[count] = analogRead(sensorPin);  
        //Serial.println( data[count]);
        count = count + 1;
          delay(10);
          if(count>300)
            break;
        }    
    //Serial.println(millis());
    //BTSerial.println(count*6);          //use bluetooth to send result to android
}
void RemoveDC(){
  Find_Minima(0);
  Find_Peak(0);
  ADC_Range = Peak_Magnitude-Minima;

  // Subtract DC (minima) 
  for (int i = 0; i < Num_Samples; i++){
     data[i] = data[i] - Minima;
  }    
  Minima = 0;  // New Minima is zero
}  
void ScaleData(){
  // Find peak value
  Find_Peak(0);
  Range = Peak_Magnitude - Minima;
  // Sclae from 1 to 1023 
  for (int i = 0; i < Num_Samples; i++){
     data[i] = 1 + ((data[i]-Minima)*1022)/Range;
  }  
  Find_Peak(0);
  Find_Minima(0);    
}
void FilterData(){
  Num_Points = 2*Moving_Average_Num+1;
  for (i = Moving_Average_Num; i < Num_Samples-Moving_Average_Num; i++){
    Sum_Points = 0;
    for(k =0; k < Num_Points; k++){   
      Sum_Points = Sum_Points + data[i-Moving_Average_Num+k]; 
    }    
  data[i] = Sum_Points/Num_Points; 
  } 
  Find_Peak(Moving_Average_Num);
  Find_Minima(Moving_Average_Num);  
}
void ComputeHeartRate(){

  // Detect Peak magnitude and minima
  Find_Peak(Moving_Average_Num);
  Find_Minima(Moving_Average_Num);
  Range = Peak_Magnitude - Minima;
  Peak_Threshold = Peak_Magnitude*Peak_Threshold_Factor;
  Peak_Threshold = Peak_Threshold/100;
  // Now detect three successive peaks 
  Peak1 = 0;
  Peak2 = 0;
  Index1 = 0;
  Index2 = 0;
  // Find first peak
  for (j = Moving_Average_Num; j < Num_Samples-Moving_Average_Num; j++){
      if(data[j] >= data[j-10] && data[j] > data[j+10] && 
         data[j] > Peak_Threshold && Peak1 == 0){
           Peak1 = data[j];
           Index1 = j; 
      }
      // Search for second peak which is at least 10 sample time far
      if(Peak1 > 0 && j > (Index1+Minimum_Peak_Separation) && Peak2 == 0){
         if(data[j] >= data[j-100] && data[j] > data[j+100] && 
         data[j] > Peak_Threshold){
            Peak2 = data[j];
            Index2 = j; 
         } 
      } // Peak1 > 0
  }  
 PR1 = abs(Index2-Index1)*Sampling_Time; // In milliseconds
 Pulse_Rate = 60000000/PR1;
}
void Find_Minima(int Num){
  Minima = 5000;
  for (int m = Num; m < Num_Samples-Num; m++){
      if(Minima > data[m]){
        Minima = data[m];
      }
  }
}

void Find_Peak(int Num){
  Peak_Magnitude = 0;
  for (int m = Num; m < Num_Samples-Num; m++){
      if(Peak_Magnitude < data[m]){
        Peak_Magnitude = data[m];
     }
  }
}
void loop() {
  // put your main code here, to run repeatedly:
   //delay(1000);
   //wifi   
  if(wifiReady){
    if(!linkok){
    sendCmd("AT+CIPSTART=\"TCP\",\"192.168.209.170\",8881");
    //sendCmd("AT+CIPSTART=\"TCP\",\"192.168.208.237\",8880");
      if(waitForResponse(1000,"OK\r\n")){
        log("link ok");
        linkok=true;   
        senseHeartRate();
        hcsr();
        if(linkok){
          if(Bump)
            senddata("-1");
          else
            senddata(String(Pulse_Rate)); 
          String re = readLine();
          if(re=="+IPD,1:b"){  
            log("bump");         
            //if(!Bump){
              log("other");
              lcd.setCursor(0,0);
              lcd.print("          ");
              lcd.setCursor(0,0);
              lcd.print("other bump");
            //}
          }
          //else if(re=="+IPD,1:h"){
              log("hh");
              dspeed();
              if((Pulse_Rate>120)||(Pulse_Rate<70)){
                lcd.setCursor(0,0);
                lcd.print("          ");
                lcd.setCursor(0,0);
                lcd.print("hearterror"); 
              //} 
            }                     
        }
      }
    }
        else{
          sendCmd("AT+CIPCLOSE");
          if(waitForResponse(5500,"ERROR\r\n"))      
            linkok=false; 
              
          }     
  }
  else{
    if(!connectToWifi())
      delay(2000);
    }
}

