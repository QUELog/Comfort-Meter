/* Corrispondenze Pin di lettura sensore - valore - led di allarme relativo
 * ANA0 = CO2 = PIN2
 * ANA1 = TEMP = PIN7
 * ANA2 = SUONO = PIN8
 * ANA3 = LUCE = PIN9
 * DIG19 = ERRORE
 */
 
//Impostazioni
#define sensorsBufferLength 60
#define sensorsReadsDelay 50
#define sensorsFiltrationDelay 5
#define errorPin 3

#include <SPI.h>
#include <Ethernet.h>

long lastUDPSend;
byte mac[] = {0x90, 0xA2, 0xDA, 0x00, 0x0C, 0xD9};//Indirizzo di mac della eth schield
int sensors[5];//Valori dei sensori
byte sensorsLeds[] = {2, 7, 8, 9};//Pin dei led di allarme dei sensori
float sensorsComfort[4];//Livello di comfort dei singoli parametri
int weight[4] = {361, 252, 205, 205};//Importanza dei parametri nella formula del comfort index
int sensorsRanges[] = {200, 1020, 200, 1020, 200, 1020, 200, 1020};//Range di valori in cui si ha comfort
int sensorsBuffer[4][sensorsBufferLength];
int sensorsBufferOrdered[4][sensorsBufferLength];
int sensorsBufferIndex = 0;
int sensorsReadsIndex = 0;
long lastSensorsRead = millis();

void setup(){
  //Inizializazione dei led di allarme dei sensori
  for(int a = 0; a < 4; a++){
    pinMode(sensorsLeds[a], OUTPUT);
    digitalWrite(sensorsLeds[a], LOW);
  }
  pinMode(errorPin, OUTPUT);
  digitalWrite(errorPin, LOW);
  //Inizializzo apparato ethernet e il server UDP
  /*if(Ethernet.begin(mac) == 0){
    digitalWrite(19, HIGH);
  } else {
    // Inizializzato con successo, inizializzo il web server
    
  }*/
  pinMode(3, OUTPUT);
}
void loop(){
  delay(1000);
  digitalWrite(errorPin, HIGH);
  delay(1000);
  digitalWrite(errorPin, LOW);
  /**** INIZIO ACQUISIZIONE DATI ****/
  if(millis() - lastSensorsRead >= sensorsReadsDelay){
    for(int a = 0; a < 4; a++){
      sensorsBuffer[a][sensorsBufferIndex] = analogRead(a);
    }
    if(++sensorsBufferIndex >= sensorsBufferLength){
      sensorsBufferIndex = 0;
    }
    sensorsReadsIndex++;
    lastSensorsRead = millis();
  }
  /**** FINE ACQUISIZIONE ****/
  /**** INIZIO FILTRO DATI ****/
  if(sensorsReadsIndex >= sensorsFiltrationDelay){
    for(int a = 0; a < 4; a++){
      for(int b = 0; b < sensorsBufferLength; b++){
        sensorsBufferOrdered[a][b] = sensorsBuffer[a][b];
      }
    }
    int bubbleSortSwap = 0;
    for(int a = 0; a < 4; a++){
      //Ordino il vettore delle letture
      int n = sensorsBufferLength;
      int p = sensorsBufferLength;
      int flag = 0;
      do{
        flag = 0;
        for(int b = 0; b < n - 1; b++){
          if(sensorsBufferOrdered[a][b] < sensorsBufferOrdered[a][b + 1]){
            bubbleSortSwap = sensorsBufferOrdered[a][b];
            sensorsBufferOrdered[a][b] = sensorsBufferOrdered[a][b + 1];
            sensorsBufferOrdered[a][b + 1] = bubbleSortSwap;
            flag = 1;
            p = b + 1;
          }
        }
        n = p;
      }while(flag == 1);
      //Ricavo il valore più frequente
      int modeValue = 0;
      int modeValue2 = sensorsBufferOrdered[a][0];
      int modeTimes = 0;
      int modeTimes2 = 1;
      for(int b = 1; b < sensorsBufferLength + 1; b++){
        if(sensorsBufferOrdered[a][b] == modeValue2 && b < sensorsBufferLength){
          modeTimes2++;
        } else {
          if(modeTimes < modeTimes2){
            modeValue = modeValue2;
            modeTimes = modeTimes2;
          }
          if(b < sensorsBufferLength){
            modeValue2 = sensorsBufferOrdered[a][b];
            modeTimes2 = 1;
          }
        }
      }
      // Prendo il valore moda e lo considero come il valore acquisito dai sensori
      sensors[a] = modeValue;
    }
    sensorsReadsIndex = 0;
  }
  /**** FINE FILTRO DATI ****/
  /**** INIZIO GESTIONE LED ****/
  for(int a = 0; a < 8; a +=2){
    //Se il valore è minore del valore ideale e minore del limite minimo, accendo il led relativo
    if((sensorsRanges[a] + sensorsRanges[a+1])/2 > sensors[a/2]){
      if(sensors[a/2] < sensorsRanges[a]){
        digitalWrite(sensorsLeds[a/2], HIGH);
        sensorsComfort[a/2] = 0;
      } else {
        digitalWrite(sensorsLeds[a/2], LOW);
        sensorsComfort[a/2] = 1;
      }
    //Sel valore è maggiore del valore ideale e maggiore del limite massimo, accendo il led relativo
    } else {
      if(sensors[a/2] > sensorsRanges[a+1]){
        digitalWrite(sensorsLeds[a/2], HIGH);
        sensorsComfort[a/2] = 0;
      } else {
        digitalWrite(sensorsLeds[a/2], LOW);
        sensorsComfort[a/2] = 1;
      }
    }
  }
  //calcolo del comfort index
  sensors[4] = 0;
  for(int a = 0; a < 4; a++){
    sensors[4] += weight[a] * sensorsComfort[a];
  }
  // Piloto lancetta in base al comfort index
  analogWrite(6, map(sensors[4], 0, 1023, 0, 255));
  /**** FINE GESTIONE LED ****/
}
