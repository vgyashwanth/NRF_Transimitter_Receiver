// Receiver Code
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <stdint.h>
#include <Servo.h>
#include <Ticker.h>

#define CE_PIN         2
#define CSN_PIN        4

#define MOTOR_PIN       3
#define SERVO_PIN      10
#define HORN_PIN       9
#define HEAD_LIGHTS    A0
#define BOOSTER_LIGHTS A1
#define RED_LIGHTS     A2
#define LEFT_INDICATOR A4
#define RIGHT_INDICATOR A3
RF24 radio(CE_PIN, CSN_PIN); // CE, CSN
Servo servo;  // create servo object to control a servo
Servo motor;  // for controlling the esc of the motor

// frequency
#define HORN_FREQUENCY 250 // frequency of the  Horn in Hz (McLaren 765LT)
#define PARKING_LIGHT_FREQ 2480
#define PARKING_LIGHT_BLINK_TIME 250 // msec
#define INDICATOR_DELAY          100 // msec  
#define BOOSTER_LIGHT_TOOGLE_TIME 50 // msec
#define STEERING_MIN_ANGLE 45 // degree
#define STEERING_MAX_ANGLE 135 // degree
 
// Ticker to create the Tasks
void ToggleBoosterLight(void);
void parkingLights_ISR(void);
void LeftIndicator_ISR(void);
void RightIndicator_ISR(void);
// To this:
// Arguments: callback function, interval, repeating times (0 = endless), resolution (MILLIS)
Ticker parkingLightTicker(parkingLights_ISR, PARKING_LIGHT_BLINK_TIME, 0, MILLIS); 
Ticker BoosterLightTicker(ToggleBoosterLight, BOOSTER_LIGHT_TOOGLE_TIME, 0, MILLIS);
Ticker LeftIndicatorTicker(LeftIndicator_ISR, INDICATOR_DELAY, 0, MILLIS);
Ticker RightIndicatorTicker(RightIndicator_ISR, INDICATOR_DELAY, 0, MILLIS);
volatile bool isParkingLightTickerActive = false;
volatile bool isBoosterLightTurnOnFirstTime = false;
volatile bool isLeftIndicatorActive = false;
volatile bool isRightIndicatorActive = false;
uint8_t gBoosterLightToggleCounter = 0;


const byte address[6] = "00001";

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
void setup() {
  Serial.begin(9600);
  // NRF Module Settings
  radio.begin();
  // (2400MHz + 100MHz) Frequency Channel
  // both Transimitter and Receiver should be on same channel
  // helps when multiple NRF modules communicating in same space
  radio.setChannel(100); 
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

  parkingLightTicker.update(); // Keeps tracking timer 1
  BoosterLightTicker.update(); // Keeps tracking timer 2
  LeftIndicatorTicker.update();
  RightIndicatorTicker.update();

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
    if(received_data.throttle > 1900)
    {
        #define TOOGLE_COUNTER_MAX 25
        if(isBoosterLightTurnOnFirstTime == false)
        {
          BoosterLightTicker.start();
          isBoosterLightTurnOnFirstTime = true;
        }
        else if(gBoosterLightToggleCounter > TOOGLE_COUNTER_MAX)
        {
          BoosterLightTicker.stop();
          digitalWrite(BOOSTER_LIGHTS, HIGH);
        }
        else
        {
          // Nothing to do
        }
    }
    else
    {   
        digitalWrite(BOOSTER_LIGHTS,LOW);
        isBoosterLightTurnOnFirstTime = false;
        gBoosterLightToggleCounter = 0;  
    }
    // Turn off the red lights if it is on already only if it is not turned on
    if(received_data.digital_ch2 == HIGH)
    {
      digitalWrite(RED_LIGHTS,LOW);
    }
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
    BoosterLightTicker.stop();
    isBoosterLightTurnOnFirstTime = false;
    gBoosterLightToggleCounter = 0;  
    digitalWrite(BOOSTER_LIGHTS,LOW);
    Serial.print("Throttle : ");
    Serial.println(received_data.throttle);
  }
  
  // Steering
  if(received_data.right_turn || received_data.left_turn)
  { 
    if((received_data.streeing < STEERING_MIN_ANGLE) || (received_data.streeing > STEERING_MAX_ANGLE))
    {
      // invalid angle
      // dont process further
      // important to avoid servo damage
      return ;
    }
    servo.write(received_data.streeing);

    if ((received_data.right_turn && (received_data.streeing > 110)) &&
              (received_data.digital_ch3 == HIGH))
    { 
      if(isRightIndicatorActive == false)
      {
        // Turn off the left indicator
        LeftIndicatorTicker.stop();
        isLeftIndicatorActive = false;
        RightIndicatorTicker.start();
      }
      isRightIndicatorActive = true;
    }
    if ((received_data.left_turn && (received_data.streeing < 70)) &&
            (received_data.digital_ch3 == HIGH))
    { 
      if(isLeftIndicatorActive == false)
      {
        LeftIndicatorTicker.start();
        // Turn off the right indicator
        RightIndicatorTicker.stop();
        isRightIndicatorActive = false;
      }
      isLeftIndicatorActive = true;
    }
    Serial.print("Steering: ");
    Serial.println(received_data.streeing);
  }
  else
  { 
    LeftIndicatorTicker.stop();
    RightIndicatorTicker.stop();
    isLeftIndicatorActive = false;
    isRightIndicatorActive = false;
    servo.write(received_data.streeing);
    Serial.print("Steering: ");
    Serial.println(received_data.streeing);
  }
  // Head Lights
  if(received_data.digital_ch1 == LOW && received_data.digital_ch2 == HIGH)
  {
    digitalWrite(HEAD_LIGHTS,HIGH);
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);
    Serial.println(received_data.digital_ch2);

  }
  else if(received_data.digital_ch1 == HIGH && received_data.digital_ch2 == LOW)
  {
    digitalWrite(HEAD_LIGHTS,HIGH);
    digitalWrite(RED_LIGHTS, HIGH);
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);
    Serial.println(received_data.digital_ch2);
  }
  else 
  {
    digitalWrite(HEAD_LIGHTS,LOW);
    // if the car is coming back, then we should keep the red light on
    if(!(received_data.backward))
    {
      digitalWrite(RED_LIGHTS, LOW);
    }
    Serial.print("Head Lights: ");
    Serial.println(received_data.digital_ch1);
    Serial.println(received_data.digital_ch2);
  }

  // process the horn
  if(received_data.digital_ch4 == LOW && received_data.digital_ch3 == HIGH)
  {
    tone(HORN_PIN, HORN_FREQUENCY, 100);
  }
  else 
  {  
    noTone(HORN_PIN);
  }

    // process the parking lights
  if(received_data.digital_ch4 == HIGH && received_data.digital_ch3 == LOW)
  {
    if(isParkingLightTickerActive == false)
    {
      parkingLightTicker.start();
      isParkingLightTickerActive = true;
    }

  }
  else
  {
      parkingLightTicker.stop();
      isParkingLightTickerActive = false;
      digitalWrite(LEFT_INDICATOR, LOW);
      digitalWrite(RIGHT_INDICATOR, LOW);
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

void parkingLights_ISR()
{
    // blink the parking lights with tone,
    static bool state = 0;
    state = state^1; // do the xor to flip the state
    digitalWrite(LEFT_INDICATOR, state);
    digitalWrite(RIGHT_INDICATOR, state);

    if(state)
    {
      tone(HORN_PIN, PARKING_LIGHT_FREQ, 1000);
    }
    else
    {
      noTone(HORN_PIN);
    }


}

void ToggleBoosterLight(void)
{
  static bool state = 0;
  state ^=1;
  digitalWrite(BOOSTER_LIGHTS,state);
  gBoosterLightToggleCounter++;

}

void LeftIndicator_ISR(void)
{

    // blink the parking lights with tone,
    static bool leftindicatorstate = 0;
    leftindicatorstate = leftindicatorstate^1; // do the xor to flip the state
    digitalWrite(LEFT_INDICATOR, leftindicatorstate);

    if(leftindicatorstate)
    {
      tone(HORN_PIN, PARKING_LIGHT_FREQ, 1000);
    }
    else
    {
      noTone(HORN_PIN);
    }

}

void RightIndicator_ISR(void)
{

    // blink the parking lights with tone,
    static bool rightindicatorstate = 0;
    rightindicatorstate = rightindicatorstate^1; // do the xor to flip the state
    digitalWrite(RIGHT_INDICATOR, rightindicatorstate);

    if(rightindicatorstate)
    {
      tone(HORN_PIN, PARKING_LIGHT_FREQ, 1000);
    }
    else
    {
      noTone(HORN_PIN);
    }

}






