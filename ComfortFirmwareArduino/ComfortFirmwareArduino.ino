/* Corrispondenze Pin di lettura sensore - valore - led di allarme relativo
 * ANA0 = CO2 = PIN2
 * ANA1 = TEMP = PIN7
 * ANA2 = SUONO = PIN8
 * ANA3 = LUCE = PIN9
 * DIG19 = ERRORE
 */

#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

//Impostazioni
#define errorPin 19
#define sensorsReadsDelay 10
#define noiseAverageBufferLength 100

// Informazioni di configurazione della NIC
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// Creo l'istanza del server HTTP
EthernetServer server(80);
EthernetClient client;
File requestedFile;
boolean isEthernetConnected = false;
boolean isSDAvailable = false;
int sensors[5];//Valori dei sensori
int noiseAverageBuffer[noiseAverageBufferLength];
int noiseAverageBufferIndex = 0;
long noiseAverage;
byte sensorsLeds[] = {2, 7, 8, 9};//Pin dei led di allarme dei sensori
float sensorsComfort[4];//Livello di comfort dei singoli parametri
int weight[4] = {361, 252, 205, 205};//Importanza dei parametri nella formula del comfort index
int sensorsRanges[] = {200, 1020, 200, 1020, 200, 1020, 200, 1020};//Range di valori in cui si ha comfort
long lastSensorsRead = millis();
IPAddress ip (192, 168, 1, 6);

void setup() {
  //Inizializzo il buffer per la media del suono
  for(int a = 0; a < noiseAverageBufferLength; a++){
    noiseAverageBuffer[a] = 0;
  }
  //Inizializazione dei led di allarme dei sensori
  for(int a = 0; a < 4; a++){
    pinMode(sensorsLeds[a], OUTPUT);
    digitalWrite(sensorsLeds[a], LOW);
  }
  pinMode(errorPin, OUTPUT);
  digitalWrite(errorPin, LOW);
  // Abilito gli SS per usare sia la NIC che l'SD reader
  pinMode(10, OUTPUT);
  pinMode(4, OUTPUT);
  // Inizializzo la parte di rete
  Ethernet.begin(mac, ip);
  isEthernetConnected = true;
  /*isEthernetConnected = Ethernet.begin(mac);
  if (!isEthernetConnected) {
    digitalWrite(errorPin, HIGH);
  }*/
  server.begin();
  //Inizializzo la parte di memorizzazione
  isSDAvailable = SD.begin(4);
  if (!isSDAvailable) {
    digitalWrite(errorPin, HIGH);
  }
}

void loop() {
  /**** INIZIO ACQUISIZIONE DATI ****/
  if(millis() - lastSensorsRead >= sensorsReadsDelay){
    for(int a = 0; a < 4; a++){
      sensors[a] = analogRead(a);
    }
    lastSensorsRead = millis();
    if(sensors[2] >= 512){
      sensors[2] -= 512;
    } else {
      sensors[2] = 0;
    }
    noiseAverageBuffer[noiseAverageBufferIndex] = sensors[2];
    if(noiseAverageBufferIndex + 1 < noiseAverageBufferLength){
      noiseAverageBufferIndex ++;
    } else {
      noiseAverageBufferIndex = 0;
    }
    noiseAverage = 0;
    for(int a = 0; a < noiseAverageBufferLength; a++){
      noiseAverage += noiseAverageBuffer[a];
    }
    sensors[2] = noiseAverage / noiseAverageBufferLength;
  }
  /**** FINE ACQUISIZIONE ****/
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
  /**** INIZIO GESTIONE RICHIESTE INFORMAZIONI ****/
  // Se si è connessi
  if (isEthernetConnected) {
    // Controllo se ci sono clients in attesa di dati
    client = server.available();
    if (client) {
      // Se l'handshake è andato a buon fine
      if (client.connected()) {
        // Memorizzo il tipo di richiesta (keyword) e l'eventuale file richiesto
        char requestKeyword [5];
        char requestedFileName [15];
        scanHTTPRequest(requestKeyword, requestedFileName);
        client.flush();
        // Rispondo alla richiesta passando il file
        // Se è richiesto il file root, cambio il nome file da / a /client.htm
        if (strcmp("/", requestedFileName) == 0) {
          strcpy(requestedFileName, "/client.htm");
        }
        // Controllo che il file esista o se è un file dinamico da generare
        if (SD.exists(requestedFileName) || strcmp(requestedFileName, "/sensors.xml") == 0) {
          // Creo l'header della risposta, personalizzandolo in base al tipo di dato passato
          client.print("HTTP/1.1 200 OK\nContent-Type: ");
          char requestedFileExtension [3];
          getFileNameExtension(requestedFileName, requestedFileExtension);
          if(strcmp(requestedFileExtension, "css") == 0){
            client.print("text/css");
          } else if (strcmp(requestedFileExtension, "xml") == 0) {
            client.print("text/xml");
          } else if (strcmp(requestedFileExtension, "js") == 0) {
            client.print("text/javascript");
          } else {
            client.print("text/html");
          }
          client.println("\nConnection: close\n");
          if (strcmp(requestedFileName, "/sensors.xml") == 0) {
            // Se il file richiesto è sensors.xml, allora invio un XML con i dati dei sensori
            client.print("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<sensors>\n\t<sensor name=\"CO2\">");
            client.print(sensors[0]);
            client.print("</sensor>\n\t<sensor name=\"temperature\">");
            client.print(sensors[1]);
            client.print("</sensor>\n\t<sensor name=\"noise\">");
            client.print(sensors[2]);
            client.print("</sensor>\n\t<sensor name=\"brightness\">");
            client.print(sensors[3]);
            client.print("</sensor>\n</sensors>");
          } else {
            // E' un file esistente nell'SD, lo apro e lo invio
            requestedFile = SD.open(requestedFileName);
            char c;
            while ((c = requestedFile.read()) > 0) {
                client.print(c);
            }
            requestedFile.close();
          }
        } else {
          // Il file non esiste, restituisco con un errore 404
          client.println("HTTP/1.1 404 Not Found\nContent-Type: text/html\nConnection: close\n");
          client.print("<!DOCTYPE HTML>\n<html><h1>Errore 404</h1><h2>Il file ");
          client.print(requestedFileName);
          client.println(" non esiste.</h2></html>");
        }
      }
      // Lascio che il browser riceva tutti i dati
      delay(1);
      // Chiudo la connessione
      client.stop();
    }
  }
  /**** FINE GESTIONE RICHIESTE INFORMAZIONI ****/
}

// Restituisce la keyword e il nome dell'eventuale file richiesto
void scanHTTPRequest (char *requestKeyword, char *requestedFileName) {
  int requestKeywordLength = client.readBytesUntil(' ', requestKeyword, 4);
  requestKeyword[requestKeywordLength] = '\0';
  int requestedFileNameLength = client.readBytesUntil(' ', requestedFileName, 14);
  requestedFileName[requestedFileNameLength] = '\0';
  return;
}

// Restituisce l'estenzione del nome file passatogli
void getFileNameExtension (char *fileName, char *extension) {
  char *fileNameDot = strchr(fileName, '.');
  *fileNameDot = ' ';
  sscanf(fileName, "%*s %s", extension);
  *fileNameDot = '.';
}
