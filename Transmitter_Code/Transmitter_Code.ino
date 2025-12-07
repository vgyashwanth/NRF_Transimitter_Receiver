// Transimitter Code 
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include<stdint.h>


#define CE_PIN 2
#define CSN_PIN 4

#define TANK1 0
#define TANK2 1

#define TANK1_SENSOR_TRIG 3
#define TANK1_SENSOR_ECHO 5
#define TANK2_SENSOR_TRIG 6
#define TANK2_SENSOR_ECHO 7


RF24 radio(CE_PIN, CSN_PIN); // CE, CS
// address of the nodes
const byte address[6] = "00001";


//Data packet
struct DataPacket
{
  uint16_t tank1_depth;
  uint16_t tank2_depth;
};

void ProcessData(struct DataPacket * Data);
uint16_t MeasureDepthInCm(bool tank);

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
  radio.setChannel(50); 
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setPayloadSize(sizeof(DataPacket));
  radio.stopListening();

  // // Hardware Settings
  pinMode(TANK1_SENSOR_TRIG, OUTPUT);
  pinMode(TANK1_SENSOR_ECHO, INPUT);
  pinMode(TANK2_SENSOR_TRIG,OUTPUT);
  pinMode(TANK2_SENSOR_ECHO,INPUT);

  

}

void loop() 
{   
  struct DataPacket data;
  ProcessData(&data);

  // send data
  radio.write(&data, sizeof(data));
  
}

void ProcessData(DataPacket* data)
{

    data->tank1_depth = MeasureDepthInCm(TANK1);
    data->tank2_depth = MeasureDepthInCm(TANK2);

}

uint16_t MeasureDepthInCm(bool tank)
{

  uint32_t duration;
  uint16_t distance;
  if(tank == TANK1)
  {
    digitalWrite(TANK1_SENSOR_TRIG, LOW);
    delayMicroseconds(5);

    digitalWrite(TANK1_SENSOR_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TANK1_SENSOR_TRIG, LOW);

    duration = pulseIn(TANK1_SENSOR_ECHO, HIGH);
    
  }  
  else
  {
    digitalWrite(TANK2_SENSOR_TRIG, LOW);
    delayMicroseconds(5);

    digitalWrite(TANK2_SENSOR_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TANK2_SENSOR_TRIG, LOW);

    duration = pulseIn(TANK2_SENSOR_ECHO, HIGH);

  }

  // Calculate the Depth in cm
  distance = (duration * 0.034 / 2);

  return distance;


}




