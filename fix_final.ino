
#include <Ethernet.h>
#include <ArduinoJson.h> //6.17.3
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <NewPing.h>


// replace the MAC address below by the MAC address printed on a sticker on the Arduino Shield 2
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

EthernetClient client;

int    HTTP_PORT   = 80;
String HTTP_METHOD = "GET";
char   HOST_NAME[] = "192.168.104.27"; // change to your PC's IP address
String PATH_NAME   = "/rfidtol/page/data-api.php";
String NAMA_TOL = "Jagorawi";
String getData;

//Ultra
int triger  = A0;
int echo    = A1;
int batas   = 19; //Maksimal 400 cm
int jarakGround = 19;
String vehicle = "none";
int tinggikendaraan = 0;

NewPing cm(triger, echo, batas);

//Servo
int servo = 6;
Servo palang;
int tutup=0;

//LED
const int pinM = 2;
const int pinK = 3;
const int pinH = 4;

//IR
const int pinIR = 5;
int jeda = 0;

//RFID
#define SS_PIN 9
#define RST_PIN 8
#define buzzer 7

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

void setup() {
  Serial.begin(115200);
  //LED
  pinMode(pinM, OUTPUT);
  pinMode(pinK, OUTPUT);
  pinMode(pinH, OUTPUT);
  digitalWrite(pinM, HIGH);
  digitalWrite(pinK, LOW);
  digitalWrite(pinH, LOW);
  // Buzzer
  pinMode (buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  //  IR
  pinMode(pinIR, INPUT);
  //Servo
  palang.attach(servo);
  palang.write(90);

  while (!Serial);
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  //delay(3000);
  //START IP DHCP
  Serial.println("Konfigurasi DHCP, Silahkan Tunggu!");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP Gagal!");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet Tidak tereteksi :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Hubungkan kabel Ethernet!");
    }
    while (true) {
      delay(1);
    }
  }
  //End DHCP
  delay(5000);
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
  client.connect(HOST_NAME, HTTP_PORT);
  Serial.println("Siap Digunakan!");
  digitalWrite(buzzer, HIGH);
  delay(50);
  digitalWrite(buzzer, LOW);
  delay(50);
  digitalWrite(buzzer, HIGH);
  delay(50);
  digitalWrite(buzzer, LOW);
  delay(50);
}

void loop() {
  //Baca data
  int bacaIR = digitalRead(pinIR);
  int bacaJarak = cm.ping_cm();
  if (bacaJarak != 0 ) {
    tinggikendaraan = jarakGround - bacaJarak;
    if ( tinggikendaraan <= 6 ) {
      vehicle = "mobil";
    } else if (tinggikendaraan >= 7 && tinggikendaraan < 9) {
      vehicle = "truk";
    } else if (tinggikendaraan >= 9 ) {
      vehicle = "bus";
    }
  } else {
    vehicle = "Maksimal";
  }

  if (bacaIR == 0 && jeda == 1) { //Jika Sensor Infrared mendeteksi dan keadaan Jeda Sudah menjadi 1
    palang.write(0);
    tutup=1;
  }
  if (bacaIR == 1 && tutup == 1 ) {
    Serial.println("------------------------------------------------------------");
    Serial.println("PALANG DITUTUP!");
    Serial.println("------------------------------------------------------------");
    palangditutup();
    jeda = 0; //Merubah Kondisi jeda
    tutup=0;
    delay(200);
  }

  if (jeda == 0 && bacaJarak != 0) {
    //Program yang akan dijalankan berulang-ulang
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
      return;
    }
    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial()) {
      return;
    }

    //Show UID on serial monitor
    Serial.println("-----------------------------------------------------");
    Serial.print("UID tag :");
    String uidString;
    byte letter;
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
      uidString.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "" : ""));
      uidString.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    //  Serial.print("Message : ");
    uidString.toUpperCase();
    Serial.println(uidString);

    //POST TO WEB
    client.connect(HOST_NAME, HTTP_PORT);
    client.println(HTTP_METHOD + " " + PATH_NAME +
                   "?rfid=" + String(uidString) +
                   "&&tol=" + String(NAMA_TOL) +
                   "&&vehicle=" + String(vehicle) +
                   //"&sensor2=" + String(sensor2) +
                   " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      if (client.available()) {
        char endOfHeaders[] = "\r\n\r\n";
        client.find(endOfHeaders);
        getData = client.readString();
        getData.trim();

        //AMBIL DATA JSON
        const size_t capacity = JSON_OBJECT_SIZE(6) + 110; //cari dulu nilainya pakai Arduino Json 5 Asisten
        DynamicJsonDocument doc(capacity);
        //        StaticJsonDocument<192> doc;
        DeserializationError error = deserializeJson(doc, getData);


//        const char* rfid_dibaca      = doc["rfid"];
//        const char* nama_dibaca      = doc["nama"];
        //        const char* tanggal_dibaca   = doc["tanggal"];
        //        const char* nama_tol_dibaca  = doc["namatol"];
        const char* status_dibaca    = doc["ceksaldo"];
        //        const char* saldoawal        = doc["saldoawal"];
        //        const char* saldoakhir       = doc["saldoakhir"];
        const char* bayar            = doc["bayar"];

        //LOGIKA
        Serial.print(getData);
        if (String(status_dibaca) == "unknown") {
          //POST TO SERIAL
          Serial.println();
          Serial.println("-----------------------------------------------------");
          Serial.println("Kartu Tidak Terdaftar!");
//          Serial.print("RFID     = "); Serial.println(rfid_dibaca);
          Serial.print("Saldo    = Rp"); Serial.println("0");
          Serial.print("-----------------------------------------------------");
          Serial.println();
          none();
          buzzergagal();
        } else if (String(status_dibaca) == "cukup" ) {
          //POST TO SERIAL
          Serial.println();
          Serial.println("-----------------------------------------------------");
          Serial.println("Silahkan Lewat");
//          Serial.print("RFID             = "); Serial.println(rfid_dibaca);
//          Serial.print("Nama             = "); Serial.println(nama_dibaca);
//          Serial.print("Tol              = "); Serial.println(NAMA_TOL);
          //               Serial.print("Waktu            = ");Serial.println(tanggal_dibaca);
          Serial.print("Bayar            = Rp"); Serial.println(bayar);
          //               Serial.print("Saldo Sebelum    = Rp");Serial.println(saldoawal);
          //               Serial.print("Saldo Sesudah    = Rp");Serial.println(saldoakhir);
          Serial.print("-----------------------------------------------------");
          Serial.println();
          buzzeroke();
          palangbuka();
          jeda = 1;
          delay(400);
        } else {
          //POST TO SERIAL
          Serial.println();
          Serial.println("-----------------------------------------------------");
          Serial.println("Saldo Tidak Cukup");
//          Serial.print("RFID             = "); Serial.println(rfid_dibaca);
//          Serial.print("Nama             = "); Serial.println(nama_dibaca);
//          Serial.print("Tol              = "); Serial.println(NAMA_TOL);
          //             Serial.print("Waktu            = ");Serial.println(tanggal_dibaca);
          Serial.print("Bayar            = Rp"); Serial.println(bayar);
          //             Serial.print("Saldo Sebelum    = Rp");Serial.println(saldoawal);
          //             Serial.print("Saldo Sesudah    = Rp");Serial.println(saldoakhir);
          Serial.print("-----------------------------------------------------");
          Serial.println();
          none();
          buzzergagal();
          //             }
        }

      }
    }
  }
}
void buzzeroke() {
  digitalWrite(buzzer, HIGH);
  delay(50);
  digitalWrite(buzzer, LOW);
  delay(50);
  digitalWrite(buzzer, HIGH);
  delay(50);
  digitalWrite(buzzer, LOW);
  delay(50);
}

void buzzergagal() {
  digitalWrite(buzzer, HIGH);
  delay(400);
  digitalWrite(buzzer, LOW);
}

void buzzerlewat() {
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
  delay(40);
  digitalWrite(buzzer, HIGH);
  delay(40);
  digitalWrite(buzzer, LOW);
}

//LED
void hijau() {
  digitalWrite(pinM, LOW);
  digitalWrite(pinK, HIGH);
  delay(300);
  digitalWrite(pinK, LOW);
  digitalWrite(pinH, HIGH);
}
void merah() {
  digitalWrite(pinH, LOW);
  digitalWrite(pinK, HIGH);
  delay(300);
  digitalWrite(pinK, LOW);
  digitalWrite(pinM, HIGH);
}

void palangbuka() {
  hijau();
  palang.write(0);
}

void palangditutup() {
  merah();
  delay(200);
  buzzerlewat();
  palang.write(90);
}
void none() {
  digitalWrite(pinM, HIGH);
  palang.write(90);
}
