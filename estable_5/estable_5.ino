//Añadimos las librerias de la SD
#include <SPI.h>
#include <SD.h>

#include <Wire.h>
#include <WireSlave.h> 

#include <Adafruit_Fingerprint.h>

//Añadimos las librerias que usaran i2C para la pantalla LCD
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);

//añadimos la librerias del Wi-fi
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <HardwareSerial.h>
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

//credenciales Wifi Colegio:
const char* ssid = "B04";
const char* passwordwifi = "tirantb04_2022";

// Configuración del servidor NTP
const char* ntpServer = "time.google.com";
const int timeZone = +2;  // Zona horaria

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, timeZone * 3600, 60000);

#define SDA_PIN 21    //Pines i2c
#define SCL_PIN 22
#define SD_CS 5 // pin de la SD

uint8_t id;

unsigned long previousMillis = 0; // Almacena la última vez que se actualizó la pantalla LCD
const unsigned long interval = 5000; // Intervalo para actualizar el mensaje en la pantalla LCD

const String password = "1234";// Contraseña del Admin de huellas

uint8_t readnumber(void) 
{
  uint8_t num = 0;

  while (num == 0) {
    while (! Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

void setup() 
{
  Serial.begin(9600);

  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("Iniciandome");
  
  

  //Conectamos con el lector micro SD. Si no conecta, el Esclavo no respoderá
  if(!SD.begin(SD_CS)) 
      {
        Serial.println("Card Mount Failed");
        while(true){};
      }
  delay(1000); //este delay si se borra, peta.
  finger.begin(57600);
  delay(100);

  WiFi.begin(ssid, passwordwifi);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();

}

void loop() 
{

timeClient.update(); // con esto refrescamos la informacion en cada ciclo

if (Serial.available()) 
  {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Elimina los espacios en blanco y caracteres de nueva línea
    if (command == "menu") 
    {
      menu();
    }
  }
  
  
  unsigned long currentMillis = millis(); // Obtiene el tiempo actual en milisegundos
  
  if (currentMillis - previousMillis >= interval) 
  {
    // Guarda el tiempo actual
    previousMillis = currentMillis;
    timeClient.update();

    // Obtención de la hora actual en formato UNIX
    unsigned long epochTime = timeClient.getEpochTime();

    // Verifica si el tiempo epoch es válido antes de continuar
    if (epochTime > 1500000000) 
      { // Comprueba si la fecha es válida (después del 14 de julio de 2017)
        // Conversión de la hora UNIX a una estructura de tiempo en C
        struct tm *timeinfo;
        timeinfo = localtime((time_t *)&epochTime);
  
        // Conversión de la estructura de tiempo a una variable de tipo "char" en el formato deseado
        char fechaHora[17];
        strftime(fechaHora, sizeof(fechaHora), "%d-%m-%Y %H:%M", timeinfo);
  
        // Impresión de la fecha y la hora actual
        imprimirMensaje(fechaHora);
      } 
    else 
      {
        Serial.println("Sincronizando...");
        Serial.print("Servidor NTP: ");
        Serial.println(ntpServer);
        delay(1000);
      }
  }
  
  
  uint8_t result = finger.getImage();
  if (result == FINGERPRINT_OK) 
  {
    Serial.println("Imagen capturada");
    result = finger.image2Tz();

    if (result == FINGERPRINT_OK) 
    {
      result = finger.fingerFastSearch();
      if (result == FINGERPRINT_OK)         // La huella coincide con una huella guardada
      {

        char mensaje[50];
        snprintf(mensaje, sizeof(mensaje), "Acceso Valido");
        imprimirMensaje(mensaje);

        String y = String(finger.fingerID);
        while(y.length() < 3)
          {
            y = "0" + y;
          }
        Serial.println(y); //Aqui vemos el ID en pantalla codificado en XXX

        String nombre = buscarNombrePorID(y);
          if (nombre != "") 
          {
            Serial.print("Nombre encontrado: ");
            Serial.println(nombre);

            // Muestra el mensaje en la pantalla LCD
            lcd.clear(); // Limpia la pantalla
            lcd.setCursor(0, 0); // Coloca el cursor en la primera columna de la primera fila
            lcd.print("Bienvenido ");
            lcd.setCursor(0, 1); // Coloca el cursor en la primera columna de la segunda fila
            lcd.print(nombre);
            delay(3000);
          } 
          else 
          {
            Serial.println("ID no encontrada.");
          }


        
        escribeLog("Entro",finger.fingerID);
      } 
      else if (result == FINGERPRINT_NOTFOUND)     // La huella no coincide con ninguna huella guardada
      {

      requestPassword(); // Solicita la contraseña
      }  
      else 
      {
        imprimirMensaje("Error al buscar la huella.");
      }
    }
  } 
  else if (result == FINGERPRINT_NOFINGER) 
    {
      // No hacer nada si no hay dedo
    } 
  else if (result == FINGERPRINT_PACKETRECIEVEERR) 
    {
      imprimirMensaje("Error de comunicación");
    } 
  else if (result == FINGERPRINT_IMAGEFAIL) 
    {
      imprimirMensaje("Error al capturar la imagen");
    } 
  else 
    {
      imprimirMensaje("Error desconocido");
    }

  delay(50); // Puedes ajustar este delay para controlar la frecuencia de lectura del sensor
}

//----------------------------------Funciones---------------------------
uint8_t getFingerprintEnroll() 
{

  int p = -1;
  Serial.print("Esperando que pongas tu huella en el sensor#"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("imagén tomada"); 
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      delay(100);
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Imagen convertida");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Quita el dedo porfavor");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Pon el mismo dedo porfavor");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Todo correcto");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Id guardada");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}
uint8_t getFingerprintID() 
{
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" con una coincidencia del "); Serial.println(finger.confidence); 

  return finger.fingerID;
}
int getFingerprintIDez() 
{
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID; 
}

void imprimirMensaje(const char *mensaje) 
    {
      Serial.println(mensaje);
      lcd.clear();
      lcd.print(mensaje);
    }
    
void requestPassword() 
{
  const unsigned long passwordTimeout = 10000; // Tiempo límite para ingresar la contraseña en milisegundos
  unsigned long startTime = millis();
  bool isTimedOut = false;
  int attempts = 0;
  const int maxAttempts = 1;

  
  Serial.println("Huella no valida");
  Serial.println("Introducir contraseña");
  while (!isTimedOut && attempts < maxAttempts) 
  {
    if (Serial.available() > 0) 
    {
      String inputPassword = Serial.readStringUntil('\n');
      inputPassword.trim();
      if (inputPassword == password) // La contraseña es correcta, agregar la nueva huella 
      {
        
        Serial.println("Que ID?");
        id = readnumber();
          if (id == 0) 
            {// ID #0 not allowed, try again!
               return;
            }
          char mensaje[50];
          snprintf(mensaje, sizeof(mensaje), "añadiendolo a la ID: %d", id);
          Serial.println(mensaje);
          Serial.print("Enrolling ID #");
          Serial.println(id);
          Serial.flush();

          //------------------------------------proceso de añadirlo a la SD/nombres------------------
          String y = String(id);
          while(y.length() < 3)
          {
            y = "0" + y;
          }

          
          String nombre;
          Serial.println("Ahora escribe el nombre: ");
            while (!Serial.available()) 
            { 
              delay(100); // Delay para evitar un bucle rápido de ejecución. 
            }
            nombre = Serial.readStringUntil('\n');
            nombre.trim(); // Limpia cualquier espacio en blanco antes y después del nombre
            Serial.println(nombre);
            escribenombres(y, nombre);



      
        while (!  getFingerprintEnroll() );
        break;
      } 
      else 
      {
        Serial.println("Contraseña incorrecta.");
        
        attempts++;
      }
    }

    
    if (millis() - startTime >= passwordTimeout)// Comprueba si ha pasado el tiempo límite
    {
      isTimedOut = true;
    }
  }

  if (isTimedOut) 
  {
    Serial.println("Tiempo Excedido");
  } 
  else if 
  (attempts >= maxAttempts) 
  {
    Serial.println("Intentos Excedidos");;
  }
}

void escribeLog(String accion, uint16_t usuario)
{
  Serial.println("voy a escribir en la SD");
  File file = SD.open("/log.txt",FILE_APPEND);
  if (!file) 
    {
      Serial.println("Error al abrir el archivo de registro");
      return;
    }
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  String formattedTime = timeClient.getFormattedTime();
  file.print(formattedTime);
  file.print(": ");
  file.print(usuario);
  file.print(": ");
  if(!file.println(accion)) 
    {
      Serial.println("Error al escribir en el archivo de registro");
    } 
  file.close();
  Serial.println("Registro guardado con éxito");
}

void menu() 
{
  String input = "";
  Serial.println("Menu de administrador:");

  while (true) 
    {
    Serial.println("Ingrese una opción (1-3) o 'salir' para finalizar:");
    Serial.println("1. Registrar una ID con nombre en la SD");
    Serial.println("2. Opción 2");
    Serial.println("3. Salir del Menu");

    while (!Serial.available()) 
    {
      delay(10); // Espera a que haya datos disponibles en el puerto serie
    }

    input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "1") 
    {
      Serial.println("Agregando nombres al SD");
      String ID, nombre;
      Serial.println("Escribe la ID: ");
      while (!Serial.available());
      ID = Serial.readString();

      Serial.println("Ahora escribe el nombre: ");
      while (!Serial.available());
      nombre = Serial.readString();

      escribenombres(ID, nombre);
    } 
    else if (input == "2") 
    {
      Serial.println("opción 2");

    }  
    else if (input == "3") 
    {
      Serial.println("Saliendo del menú de administrador.");
      break; // Salir del bucle while y volver al bucle principal (loop)
    } 
    else 
    {
      Serial.println("Opción no válida, por favor intente de nuevo.");
    }
  }
}

void escribenombres( String ID , String nombre)
{
  ID.trim(); // Elimina los caracteres de nueva línea y espacios en blanco de la entrada ID
  nombre.trim(); // Elimina los caracteres de nueva línea y espacios en blanco de la entrada nombre
  
  File file = SD.open("/nombres.txt",FILE_APPEND); //con esto se añade al final de la linea del archivo, no lo escribe encima
  String registro = ID +"_"+ nombre + "\n";
  file.print(registro);
  file.flush();
  file.close();
}

String buscarNombrePorID(String ID) 
{
  File file = SD.open("/nombres.txt", FILE_READ);
  if (file) {
    while (file.available()) {
      String registro = file.readStringUntil('\n');
      int indiceGuion = registro.indexOf('_');
      String idActual = registro.substring(0, indiceGuion);
      if (idActual == ID) {
        file.close();
        return registro.substring(indiceGuion + 1, registro.length());
      }
    }
    file.close();
  }
  return "";
}
