#include "digitalWriteFast.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include<Wire.h>
#include<math.h>

// creating object of class MPU6050
// class default I2C address is 0x68
MPU6050 mpu;

int n = 1;
int m = 1;

float comp_alpha = 0.03;
float roll = 0;

//accelerometer and gyroscope readings
int16_t ax, ay, az;
int16_t gx, gy, gz, gnx, gny, gnz;
float a[3] = {0, 0, 0};
float g[3] = {0, 0, 0};

//filter constants
float pi = 3.14;
float f_cut = 5;
float dT = 0.01;
float Tau = (1 / (2 * pi*f_cut));            // Time constant
float alpha = (Tau / (Tau + dT));

//scaling factor
float accel_sf = 16384;
float gyro_sf = 131;

//variables for filter
float lpx = 0;
float lpy = 0;
float lpz = 0;
float hpx = 0;
float hpy = 0;
float hpz = 0;

// left motor pins
#define InAL            10                      // INA motor pin
#define InBL            11                      // INB motor pin
#define PWML            6                       // PWM motor pin
#define encodPinAL      2                       // encoder A pin
#define encodPinBL      3                       // encoder B pin

//right motor pins
#define InAR            12                      // INA motor pin
#define InBR            13                      // INB motor pin
#define PWMR            5                       // PWM motor pin
#define encodPinAR      18                       // encoder A pin
#define encodPinBR      19                       // encoder B pin

unsigned long start_Time = millis();
long prevtime=0;
volatile int countLeft = 0;
volatile int countRight = 0;
volatile int left_prev_count = 0;
volatile int right_prev_count = 0;
volatile int left_RPM = 0;
volatile int right_RPM = 0;
int leftOffset = 0;
int rightOffset = 0;
float konstant = 11.11/2;
float velocity = 0;
float eeta = 0.85;

//Timer<1,millis> timer;

void read_accel() {
  // read raw accel readings
  mpu.getAcceleration(&ax, &ay, &az);

  a[0] = (ax / accel_sf);
  a[1] = (ay / accel_sf);
  a[2] = (az / accel_sf);
  // Serial.println(a[0]);
  low_pass_filter(a[0], a[1], a[2]);

}

void read_gyro() {
  //read raw gyro raedings

  mpu.getRotation(&gx, &gy, &gz);

  g[0] = (gx / gyro_sf);
  g[1] = (gy / gyro_sf);
  g[2] = (gz / gyro_sf);

  high_pass_filter(g[0], g[1], g[2]);

}
void low_pass_filter(float Ax, float Ay, float Az)
{
  if (n == 1) {
    lpx = (1 - alpha) * Ax;
    lpy = (1 - alpha) * Ay;
    lpz = (1 - alpha) * Az;
    n++;
  }
  else
  {
    lpx = (1 - alpha) * Ax + alpha * lpx;
    lpy = (1 - alpha) * Ay + alpha * lpy;
    lpz = (1 - alpha) * Az + alpha * lpz;
  }
}
void high_pass_filter(float Gx, float Gy, float Gz)
{
  if (m == 1)
  {
    hpx = (1 - alpha) * Gx;
    hpy = (1 - alpha) * Gy;
    hpz = (1 - alpha) * Gz;
    m++;
  }
  else
  {
    hpx = (1 - alpha) * (hpx + Gx - gnx);
    hpy = (1 - alpha) * (hpy + Gy - gny);
    hpz = (1 - alpha) * (hpz + Gz - gnz);
  }
  gnx = Gx;
  gny = Gy;
  gnz = Gz;
}

void complimentary_filter_roll()
{
  roll = (1 - comp_alpha) * (roll - g[1] * dT) + (comp_alpha) * (atan(a[0] / abs(a[2]))) * (180 / 3.14);
}

// Function to test the connections
void test() {
  Serial.println("Testing device connections...");
  Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
}

// Function to set offsets obtained by running IMU_zero example code
void set_offsets() {

  //to set accelerometer offsets
  mpu.setXAccelOffset(-5699);
  mpu.setYAccelOffset(-645);
  mpu.setZAccelOffset(1237);

  //to set gyro offsets
  mpu.setXGyroOffset(-104);
  mpu.setYGyroOffset(15);
  mpu.setZGyroOffset(-10);
}


void setup() 
{

// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin();
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif
  // initializing serial communication
  Serial.begin(9600);
  // initialising mpu sensor
  mpu.initialize();
  // To test the connections
  test();
  mpu.setFullScaleAccelRange(0);
  mpu.setFullScaleGyroRange(0);
  // To set the offsets
  set_offsets();
  
  // put your setup code here, to run once:
  pinMode(InAL, OUTPUT);
  pinMode(InBL, OUTPUT);
  pinMode(PWML, OUTPUT);
  pinMode(encodPinAL, INPUT_PULLUP);
  pinMode(encodPinBL, INPUT);
  digitalWrite(encodPinAL, HIGH);                      // turn on pullup resistor
  attachInterrupt(digitalPinToInterrupt(2), ISRL, CHANGE);

  pinMode(InAR, OUTPUT);
  pinMode(InBR, OUTPUT);
  pinMode(PWMR, OUTPUT);
  pinMode(encodPinAR, INPUT_PULLUP);
  pinMode(encodPinBR, INPUT);
  digitalWrite(encodPinAR, HIGH);                      // turn on pullup resistor
  attachInterrupt(digitalPinToInterrupt(18), ISRR, CHANGE);
  Serial.begin(9600);
  
}


/*void isr20ms()
{
  // Make a local copy of the global encoder count
  volatile float current_countLeft = countLeft;
  volatile float current_countRight = countRight;
  
  //     (Change in encoder count) * (60 sec/1 min)
  // RPM = __________________________________________
  //     (Change in time --> 20ms) * (PPR --> 840)
  
  left_RPM = (float)(((current_countLeft - left_prev_count) * 60)/(0.02*270));
  right_RPM = (float)(((current_countRight - right_prev_count) * 60)/(0.02*270));
  
  // Store current encoder count for next iteration
  left_prev_count = current_countLeft;
  right_prev_count = current_countRight;
  

  
  
}*/


void ISRL()
{
  int state = digitalReadFast(encodPinAL);
  if (digitalReadFast(encodPinBL))
  {
    state ? countLeft-- : countLeft++;
    
  }
  else
  {
    state ? countLeft++ : countLeft--;
  }
 }

void ISRR()
{
  int state = digitalReadFast(encodPinAR);
  if (digitalReadFast(encodPinBR))
    state ? countRight++ : countRight--;
  else
    state ? countRight-- : countRight++;}

void moveForward() {
  //analogWrite(PWML, 255);
  //analogWrite(PWMR, 255);
  digitalWrite(InAL, HIGH);
  digitalWrite(InBL, LOW);
  digitalWrite(InAR, HIGH);
  digitalWrite(InBR, LOW);
}

void  moveBackward() {
  //analogWrite(PWML, 255);
  //analogWrite(PWMR, 255);
  digitalWrite(InAL, LOW);
  digitalWrite(InBL, HIGH);
  digitalWrite(InAR, LOW);
  digitalWrite(InBR, HIGH);
}

void stopMotor(){
  digitalWrite(InAL, LOW);
  digitalWrite(InBL, LOW);
  digitalWrite(InAR, LOW);
  digitalWrite(InBL, LOW); 
}

void moveMotor(int Left, int Right)
{
  if(Left>0 && Right>0){
    analogWrite(PWML,Left);
    analogWrite(PWMR,Right);
    moveForward();
  }
  else if(Left<0 && Right<0){
  analogWrite(PWML,-1*Left);
  analogWrite(PWMR,-1*Right);
  moveBackward();
  }

   else if(Left==0 || Right==0)
   {
    stopMotor();
  
  }

}

void botVelocity(){
  velocity = (left_RPM + right_RPM)/2.0;  
}

int getleft_RPM()
  {
      left_RPM = countLeft*konstant;
      Serial.print("Left RPM:");
      Serial.println(left_RPM);
      countLeft = 0;
    return left_RPM;
  }
   
int getRight_RPM()
  {
    right_RPM = countRight*konstant;
    Serial.print("Right RPM:");
    Serial.println(right_RPM);
    countRight = 0;
        
  }

void loop() 
{
  read_accel();
  read_gyro();
  complimentary_filter_roll();

  Serial.print(roll);
  
  moveMotor(255 + leftOffset,255 + rightOffset);
  if(millis()- prevtime>=20)
  {
    left_RPM = countLeft*konstant;
    right_RPM = countRight*konstant;
    Serial.print("Left RPM:");
    Serial.println(left_RPM);
    Serial.print("Right RPM:");
    Serial.println(right_RPM);
    Serial.print("\n");
    countLeft = 0;
    countRight = 0;
    prevtime=millis();
  }

    leftOffset = (255/velocity)*(velocity - left_RPM);
    rightOffset = (255/velocity)*(velocity - right_RPM);
  //leftOffset = eeta*(velocity - left_RPM);
  //rightOffset = eeta*(velocity - right_RPM); 
  //Serial.print("difference");
  //Serial.println(countLeft-countRight);
  
  /*Serial.print("LC");
  Serial.println(countLeft);
  Serial.print("RC");
  Serial.println(countRight);*/
  
  
}  
