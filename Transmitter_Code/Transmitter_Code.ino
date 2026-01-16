// Transimitter Code
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <stdint.h>
#include <dht.h>
#include <NewPing.h>

#define CE_PIN 2
#define CSN_PIN 4

#define TANK1 0
#define TANK2 1

#define TANK1_SENSOR_TRIG 8
#define TANK1_SENSOR_ECHO 5
#define TANK2_SENSOR_TRIG 6
#define TANK2_SENSOR_ECHO 7
#define MAX_DISTANCE 400
NewPing tank1sensor(TANK1_SENSOR_TRIG, TANK1_SENSOR_ECHO, MAX_DISTANCE);
NewPing tank2sensor(TANK2_SENSOR_TRIG, TANK2_SENSOR_ECHO, MAX_DISTANCE);

float t1_tempval1;
float t1_tempval2;
uint8_t t1_iterations = 10;

float t2_tempval1;
float t2_tempval2;
uint8_t t2_iterations = 10;

#define DHT_SENSOR_PIN 3

RF24 radio(CE_PIN, CSN_PIN); // CE, CS
dht DHT;                     // Creates a DHT object
// address of the nodes
const byte address[6] = "00001";

// Data packet
struct DataPacket
{
  uint16_t tank1_depth;
  uint16_t tank2_depth;
  float temperature;
  float humidity;
};

void ProcessData(struct DataPacket *Data);
uint16_t MeasureDepthInCm(bool tank);
float MeasureTempC(void);
float MeasureHumidity(void);

void setup()
{
  Serial.begin(9600);

  // // !!!Warning!!! dont remove this
  // // lead to Hardware Error
  // analogReference(EXTERNAL); // use AREF for reference voltage

  // NRF Settings
  radio.begin();
  // (2400MHz + 50MHz) Frequency Channel
  // both Transimitter and Receiver should be on same channel
  // helps when multiple NRF modules communicating in same space
  radio.setChannel(120);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_HIGH);
  radio.powerUp();
  radio.setPayloadSize(sizeof(DataPacket));
  radio.stopListening();
}

void loop()
{
  struct DataPacket data;

  ProcessData(&data);

  Serial.print("Tank1 : ");
  Serial.print(data.tank1_depth);
  Serial.print(" cm,  ");
  Serial.print("Tank2 : ");
  Serial.print(data.tank2_depth);
  Serial.print(" cm |");
  Serial.print("Temperature : ");
  Serial.print(data.temperature);
  Serial.print(" C |");
  Serial.print("Humidity : ");
  Serial.print(data.humidity);
  Serial.println(" %");

  // send data
  radio.write(&data, sizeof(data));
}

void ProcessData(DataPacket *data)
{

  data->tank1_depth = MeasureDepthInCm(TANK1);
  data->tank2_depth = MeasureDepthInCm(TANK2);
  data->temperature = MeasureTempC();
  data->humidity = MeasureHumidity();
}

uint16_t MeasureDepthInCm(bool tank)
{

  uint16_t depthInCm;

  if (tank == TANK1)
  {
    t1_tempval1 = ((tank1sensor.ping_median(t1_iterations) / 2) * 0.0343);

    if (t1_tempval1 - t1_tempval2 > 60 || t1_tempval1 - t1_tempval2 < -60)
    {
      t1_tempval2 = (t1_tempval1 * 0.02) + (t1_tempval2 * 0.98);
    }
    else
    {
      t1_tempval2 = (t1_tempval1 * 0.4) + (t1_tempval2 * 0.6);
    }
    depthInCm = t1_tempval2;
  }
  else
  {

    t2_tempval1 = ((tank2sensor.ping_median(t2_iterations) / 2) * 0.0343);

    if (t2_tempval1 - t2_tempval2 > 60 || t2_tempval1 - t2_tempval2 < -60)
    {
      t2_tempval2 = (t2_tempval1 * 0.02) + (t2_tempval2 * 0.98);
    }
    else
    {
      t2_tempval2 = (t2_tempval1 * 0.4) + (t2_tempval2 * 0.6);
    }
    depthInCm = t2_tempval2;
  }

  return depthInCm;
}

float MeasureTempC(void)
{

  int readData = DHT.read22(DHT_SENSOR_PIN); // DHT22/AM2302

  float temp = DHT.temperature; // Gets the values of the temperature

  return temp;
}

float MeasureHumidity(void)
{

  int readData = DHT.read22(DHT_SENSOR_PIN); // DHT22/AM2302

  float humidity = DHT.humidity; // Gets the values of the temperature

  return humidity;
}
