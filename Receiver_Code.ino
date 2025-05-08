// Receiver Code
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <stdint.h>
#include <Servo.h>

#define CE_PIN         2
#define CSN_PIN        4

#define MOTOR_PIN       3
#define SERVO_PIN      10
#define HEAD_LIGHTS    A0
#define BOOSTER_LIGHTS A1
#define RED_LIGHTS     A2
#define LEFT_INDICATOR A4
#define RIGHT_INDICATOR A3
RF24 radio(CE_PIN, CSN_PIN); // CE, CSN
Servo servo;  // create servo object to control a servo
Servo motor;  // for controlling the esc of the motor

const byte address[6] = "00001";
volatile bool left_indicator_state = 0;
volatile bool right_indicator_state = 0;

//Data packet
struct DataPacket
{
  uint16_t throttle;
  uint8_t streeing;
  uint8_t head_pot;
  uint8_t down_trim_pot;
  uint8_t top_trim_pot;
  uint8_t digital_ch1 : 1;
  uint8_t digital_ch2 : 1;
  uint8_t digital_ch3 : 1;
  uint8_t digital_ch4 : 1;
  uint8_t left_turn   : 1;
  uint8_t right_turn  : 1;
  uint8_t forward     : 1;
  uint8_t backward    : 1;
};

void Control(DataPacket received_data);
void ResetSystem(void);
void ToggleLeftIndicator();
void ToggleRightIndicator();
void ToggleBothIndicators();
void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.setChannel(100); // 2400MHz + 100MHz
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.setPayloadSize(sizeof(DataPacket));
  radio.startListening();
 
  // Hardware Setup

  servo.attach(SERVO_PIN);
  motor.attach(MOTOR_PIN);
  pinMode(HEAD_LIGHTS, OUTPUT);
  pinMode(BOOSTER_LIGHTS, OUTPUT);
  pinMode(RED_LIGHTS, OUTPUT);
  pinMode(LEFT_INDICATOR, OUTPUT);
  pinMode(RIGHT_INDICATOR, OUTPUT);

  // Motor ESC Calibration
  motor.writeMicroseconds(1500);
  delay(2000);
  
}

void loop()
{
  
  DataPacket data;
  if (radio.available()) 
  {
    radio.read(&data, sizeof(data));
    Control(data);
  }
  else
  { // 
    Serial.println("System reset");
    ResetSystem();
  }
}

void Control(DataPacket received_data)
{
  // Throttle
  if(received_data.forward && !(received_data.backward))
  {
  
    motor.writeMicroseconds(received_data.throttle);
    // Setting up the Blue Lights
    if(received_data.throttle > 1950)
    {
        digitalWrite(BOOSTER_LIGHTS,HIGH);
    }
    else
    {
        digitalWrite(BOOSTER_LIGHTS,LOW);
    }
    // Turn off the red lights if it is on already
    digitalWrite(RED_LIGHTS,LOW);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);
  }
  else if (!(received_data.forward) && received_data.backward)
  {
   
    motor.writeMicroseconds(received_data.throttle);
    // Turn off the booster lights if it is on already
    // Turn on the red lights
    digitalWrite(BOOSTER_LIGHTS,LOW);
    digitalWrite(RED_LIGHTS,HIGH);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);

  }
  else
  {
    motor.writeMicroseconds(1500); // stop the motor
    // Turn off the booster lights if it is on already
    // Turn off the red lights if it is on already
    digitalWrite(BOOSTER_LIGHTS,LOW);
    digitalWrite(RED_LIGHTS,LOW);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);
  }
  
  // Steering
  if(received_data.right_turn || received_data.left_turn)
  {
    servo.write(received_data.streeing);

    if ((received_data.right_turn && (received_data.streeing > 115)) &&
              (received_data.digital_ch3 == HIGH))
    {
      ToggleRightIndicator();
    }
    else if(received_data.digital_ch3 == HIGH)
    {
      digitalWrite(RIGHT_INDICATOR, LOW);
    }
    if ((received_data.left_turn && (received_data.streeing < 65)) &&
            (received_data.digital_ch3 == HIGH))
    { 
      ToggleLeftIndicator();
    }
    // if all indicators blinking turned off 
    else if(received_data.digital_ch3 == HIGH)
    {
      digitalWrite(LEFT_INDICATOR, LOW);
    }
    Serial.print("Steering: ");
    Serial.println(received_data.streeing);
  }
  else
  {
    servo.write(received_data.streeing);
    Serial.print("Steering: ");
    Serial.println(received_data.streeing);
  }
  if(received_data.digital_ch1 == LOW)
  {
    digitalWrite(HEAD_LIGHTS,HIGH);
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);

  }
  else
  {
    digitalWrite(HEAD_LIGHTS,LOW);
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);
  }
  // Toggling indicator lights
  if(received_data.digital_ch3 == LOW)
  {
    ToggleBothIndicators();
    Serial.print("Indicators state: ");
    Serial.println(received_data.digital_ch3);

  }
  else
  {
    digitalWrite(LEFT_INDICATOR,LOW);
    digitalWrite(RIGHT_INDICATOR,LOW);
    Serial.print("Indicators state: ");
    Serial.println(received_data.digital_ch3);
  }



}
void ResetSystem(void)
{
 
  motor.writeMicroseconds(1500); // stop the motor

  // Resetting steering
  servo.write(90);

  // Resetting all Lights
  digitalWrite(HEAD_LIGHTS,LOW);
  digitalWrite(BOOSTER_LIGHTS,LOW);
  digitalWrite(RED_LIGHTS,LOW);
  digitalWrite(LEFT_INDICATOR,LOW);
  digitalWrite(RIGHT_INDICATOR,LOW);

}

void ToggleLeftIndicator()
{
  digitalWrite(LEFT_INDICATOR,HIGH);
  delay(50);
  digitalWrite(LEFT_INDICATOR,LOW);
  delay(50);
}

void ToggleRightIndicator()
{
  digitalWrite(RIGHT_INDICATOR,HIGH);
  delay(50);
  digitalWrite(RIGHT_INDICATOR,LOW);
  delay(50);
}

void ToggleBothIndicators()
{
  digitalWrite(LEFT_INDICATOR,HIGH);
  digitalWrite(RIGHT_INDICATOR,HIGH);
  delay(50);
  digitalWrite(LEFT_INDICATOR,LOW);
  digitalWrite(RIGHT_INDICATOR,LOW);
  delay(50);

}




