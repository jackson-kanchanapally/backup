#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "DHT.h"
#define DHTPIN D1
#define DHTTYPE DHT11
#define RAIN_SENSOR_PIN D3
const int pulsePin = A0;              // Pulse Sensor purple wire connected to analog pin A0

// Volatile Variables, used in the interrupt service routine!
volatile int BPM;                  // int that holds raw Analog in 0. updated every 2mS
volatile int Signal;               // holds the incoming raw data
volatile int IBI = 600;            // int that holds the time interval between beats! Must be seeded! 
volatile boolean Pulse = false;    // "True" when User's live heartbeat is detected. "False" when not a "live beat". 
volatile boolean QS = false;       // becomes true when Arduino finds a beat.

volatile int rate[10];             // array to hold last ten IBI values
volatile unsigned long sampleCounter = 0;   // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;    // used to find IBI
volatile int P = 512;              // used to find peak in pulse wave, seeded
volatile int T = 512;              // used to find trough in pulse wave, seeded
volatile int thresh = 525;         // used to find instant moment of heart beat, seeded
volatile int amp = 100;            // used to hold amplitude of pulse waveform, seeded
volatile boolean firstBeat = true; // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = false; 
int a;// used to seed rate array so we startup with reasonable BPM

os_timer_t timer;
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
// #define WIFI_SSID "LAPTOP-EK537TSH 4632"
// #define WIFI_PASSWORD "onepiece"
#define WIFI_SSID "ACT101511044984"
#define WIFI_PASSWORD "39197540"
DHT dht(DHTPIN, DHTTYPE);
#define API_KEY "AIzaSyALcg6utMHov-wQwTfgqzUTSetnbd9AKFM"
// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://major-757a9-default-rtdb.firebaseio.com/" 
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

//unsigned long sendDataPrevMillis = 0;
//int count = 0;
bool signupOK = false;

void setup()
{
    Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  pinMode(DHTPIN, INPUT);     
  pinMode(RAIN_SENSOR_PIN, INPUT);
  dht.begin();      // we agree to talk fast!
  os_timer_setfn(&timer, timer_interrupt_handler, NULL);
  os_timer_arm(&timer, 5, true); // every 2 milliseconds
}

void loop()
{
  int sensorValue = digitalRead(RAIN_SENSOR_PIN);
  float t = dht.readTemperature();
  float tFahrenheit = (t * 9 / 5) + 32;
  if (QS == true) // A Heartbeat Was Found
  {     
   a = SerialOutputWhenBeatHappens(); 
    QS = false; // reset the Quantified Self flag for next time    
  }
  
  delay(20); //  take a break
  // Serial.print("hello ");
  // Serial.println(a);
    if ((WiFi.status() == WL_CONNECTED))
  {
    if (Firebase.ready() && signupOK ) {

       if (Firebase.RTDB.setString(&fbdo, "users/123/heartRate", a)){
       Serial.print("Heart: ");
       Serial.println(a);
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON 1: " + fbdo.errorReason());
    }
     if (Firebase.RTDB.setString(&fbdo, "users/123/water", sensorValue)){
       Serial.print("Water: ");
       Serial.println(sensorValue);
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON 2: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setString(&fbdo, "users/123/temperature", tFahrenheit)){
       Serial.print("Temp: ");
       Serial.println(tFahrenheit);
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON 3: " + fbdo.errorReason());
    }
    }
    else{
       Serial.println("could not connect");
    }
    }
}

int SerialOutputWhenBeatHappens()
{    
  
  return BPM;
}

void sendDataToSerial(char symbol, int data )
{
  Serial.print(symbol);
  Serial.println(data);                
}

void timer_interrupt_handler(void *arg)
{  
  Signal = analogRead(pulsePin);              // read the Pulse Sensor 
  sampleCounter += 2;                         // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime;       // monitor the time since the last beat to avoid noise
                                              //  find the peak and trough of the pulse wave
  if (Signal < thresh && N > (IBI/5)*3) // avoid dichrotic noise by waiting 3/5 of last IBI
  {      
    if (Signal < T) // T is the trough
    {                        
      T = Signal; // keep track of lowest point in pulse wave 
    }
  }

  if (Signal > thresh && Signal > P)
  {          // thresh condition helps avoid noise
    P = Signal;                             // P is the peak
  }                                        // keep track of highest point in pulse wave

  //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
  // signal surges up in value every time there is a pulse
  if (N > 250)
  {                                   // avoid high frequency noise
    if ( (Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3) )
    {        
      Pulse = true;                               // set the Pulse flag when we think there is a pulse
      IBI = sampleCounter - lastBeatTime;         // measure time between beats in mS
      lastBeatTime = sampleCounter;               // keep track of time for next pulse
  
      if (secondBeat)
      {                        // if this is the second beat, if secondBeat == TRUE
        secondBeat = false;                  // clear secondBeat flag
        for(int i=0; i<=9; i++) // seed the running total to get a realistic BPM at startup
        {             
          rate[i] = IBI;                      
        }
      }
  
      if (firstBeat) // if it's the first time we found a beat, if firstBeat == TRUE
      {                         
        firstBeat = false;                   // clear firstBeat flag
        secondBeat = true;                   // set the second beat flag
        return;                              // IBI value is unreliable so discard it
      }   
      // keep a running total of the last 10 IBI values
      word runningTotal = 0;                  // clear the runningTotal variable    

      for(int i=0; i<=8; i++)
      {                // shift data in the rate array
        rate[i] = rate[i+1];                  // and drop the oldest IBI value 
        runningTotal += rate[i];              // add up the 9 oldest IBI values
      }

      rate[9] = IBI;                          // add the latest IBI to the rate array
      runningTotal += rate[9];                // add the latest IBI to runningTotal
      runningTotal /= 10;                     // average the last 10 IBI values 
      BPM = 60000/runningTotal;               // how many beats can fit into a minute? that's BPM!
      QS = true;                              // set Quantified Self flag 
      // QS FLAG IS NOT CLEARED INSIDE THIS ISR
    }                       
  }

  if (Signal < thresh && Pulse == true)
  {   // when the values are going down, the beat is over
    Pulse = false;                         // reset the Pulse flag so we can do it again
    amp = P - T;                           // get amplitude of the pulse wave
    thresh = amp/2 + T;                    // set thresh at 50% of the amplitude
    P = thresh;                            // reset these for next time
    T = thresh;
  }

  if (N > 2500)
  {                           // if 2.5 seconds go by without a beat
    thresh = 512;                          // set thresh default
    P = 512;                               // set P default
    T = 512;                               // set T default
    lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date        
    firstBeat = true;                      // set these to avoid noise
    secondBeat = false;                    // when we get the heartbeat back
  }
} // end timer_interrupt_handler
