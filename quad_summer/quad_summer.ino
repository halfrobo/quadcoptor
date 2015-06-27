#include <ApplicationMonitor.h>
#include <EEPROM.h>
#include <EnableInterrupt.h>
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>
#include <PID_v1.h>
#include <Servo.h>
#include <avr/pgmspace.h>
#include <MemoryFree.h>
#include <stdarg.h>//for sprintf


#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include <Wire.h>
#endif
#define OUTPUT_READABLE_YAWPITCHROLL

Watchdog::CApplicationMonitor ApplicationMonitor;

Servo m1, m2, m3, m4;

int base_speed = 1330, max_speed = 1900, min_speed = 1000;
int esc_problem_threashold=1400;

int m1_speed, m2_speed, m3_speed, m4_speed;

int m1_speed_off = 0, m2_speed_off = 0, m3_speed_off = 0, m4_speed_off = 0;

int gyro_ypr[3];
int angle_pid_result[3];
int rate_pid_result[3];
int desired_angle[3]={0,0,0};
int desired_yaw;

float serial_ratio = 0, serial_enter, serial_leave, serial_count = 0;
float loop_ratio = 0, loop_enter, loop_leave, loop_count = 0;
float pid_ratio = 0, pid_enter, pid_leave, pid_count = 0;
float motor_ratio = 0, motor_enter, motor_leave, motor_count = 0;
float mpu_ratio = 0, mpu_enter, mpu_leave, mpu_count = 0;
float interpolate_ratio = 0, interpolate_enter, interpolate_leave, interpolate_count = 0;



String in_str, in_key, in_value, in_index, serial_send = "";

bool in_str_arr = false, show_speed = false, show_ypr = false, enable_motors = false,
	enable_pitch = false, enable_yaw = false, enable_roll = false, show_diff = false;

int count_check_serial=0,count_serial=0;

MPU6050 mpu;

int ch1=0,ch2=0,ch3=0,ch4=0,ch5=0,ch6=0;
int ch3_old=0;
volatile int count_ch5=0;
volatile int ch1_val=0,ch2_val=0,ch3_val=0,ch4_val=0,ch5_val=0,ch6_val=0;
volatile unsigned long prev_ch_update;
unsigned long prev_ch_update_copy;
int receiver_lost_threashold=100;//in ms
volatile int ch1_prev=0,ch2_prev=0,ch3_prev=0,ch4_prev=0,ch5_prev=0,ch6_prev=0;
bool ch_changed=false;

const int CH1_PIN=13;
const int CH2_PIN=12;
const int CH3_PIN=11;
const int CH4_PIN=10;
const int CH5_PIN=50;
const int CH6_PIN=52;
const int DMP_INT_PIN=2;

const int CH1_MAX=1808;
const int CH1_MIN=1208;
const int CH2_MAX=1820;
const int CH2_MIN=1216;
const int CH3_MAX=2020;
const int CH3_MIN=1016;
const int CH4_MAX=1808;
const int CH4_MIN=1208;
const int CH5_MAX=2000;
const int CH5_MIN=1000;
const int CH6_MAX=2000;
const int CH6_MIN=1000;

int CH1_EFFECT=10;
int CH2_EFFECT=30;
int CH3_MIN_EFFECT=1500;
int CH3_MAX_EFFECT=1700;
int CH4_EFFECT=30;
const int CH5_EFFECT=100;
const int CH6_EFFECT=100;
const int CH3_MIN_CUTOFF=50;

int take_off_count=0;
int take_down_count=0;
unsigned long take_down_start=0;
const int take_down_cutoff=1500;
const int take_off_gradient=100;
const int take_down_gradient=14;
const int take_down_diff=20;

byte sregRestore;

const int NUM_SAMPLES_FOR_YAW_AVERAGE=40;
const unsigned long TIME_TILL_PROPER_YAW=15000UL;
const float YAW_AVERAGE_RETAIN=0.7;

/* #define LED_PIN 13// (Arduino is 13, Teensy is 11, Teensy++ is 6) */
const int GYRO_RATIO=1;
const int YPR_RATIO=128;
const int AUTO_CYCLES=5;
const int MAX_BUFFER=5;
const float Q_RETAIN=0.5;

bool blinkState = false;

// MPU control/status vars
bool dmp_ready = false;  // set true if DMP init was successful
uint8_t mpu_int_status;   // holds actual interrupt status byte from MPU
uint8_t dev_status;      // return status after each device operation (0 = success, !0 = error)
uint16_t packet_size;    // expected DMP packet size (default is 42 bytes)
uint16_t fifo_count;     // count of all bytes currently in FIFO
uint8_t fifo_buffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q, q_past, q_res;           // [w, x, y, z]         quaternion container

VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aa_real;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaW_world;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector

int rate_ypr[3];
float gyroPitch, gyroRoll;
int32_t prev_gyro_diff[3], sum_gyro[3];
float euler[3];										// [psi, theta, phi]    Euler angle container
float ypr[3], ypr_deg[3], ypr_past[3];				// [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
int ypr_int_offset[3]={0,-5,0};
int int_angle[3];
int int_rate[3];
int gyro_int_offset[3]={-21,-8,8};
int gyro_int_raw[3];
float gyro_retain[3]={0.3,0.3,0.3};
int16_t gx, gy, gz;

// packet structure for InvenSense teapot demo
// uint8_t teapotPacket[14] = { '$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, '\r', '\n' };

void pid_update(void);
void esc_update(void);
void get_data(void);

int ESC_1=4;
int ESC_2=5;
int ESC_3=6;
int ESC_4=7;
int ESC_MIN=1000;
int ESC_MAX=2000;

volatile bool mpu_interrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmp_data_ready() {
    mpu_interrupt = true;
}

//Setup functions
void mpu_init();
void esc_init();
void rc_init();
void pid_init();
void reset_motor();

//Loop functions
void rc_update();
void ypr_update();
void check_serial();

void ch1_change();
void ch2_change();
void ch3_change();
void ch4_change();
void ch5_change();
void ch6_change();

void setup(){
    ApplicationMonitor.DisableWatchdog();
    Serial1.begin(9600);
    Serial.begin(9600);
    while (!Serial1); // wait for Leonardo enumeration, others continue immediately
    while (!Serial); // wait for Leonardo enumeration, others continue immediately

    mpu_init();
    esc_init();
    rc_init();
    pid_init();

	/* mpu_enter = mpu_leave = micros(); */
	/* serial_enter = serial_leave = micros(); */
	/* loop_enter = loop_leave = micros(); */
	/* pid_enter = pid_leave = micros(); */
	/* interpolate_enter = interpolate_leave = micros(); */
    ApplicationMonitor.Dump(Serial);
    ApplicationMonitor.EnableWatchdog(Watchdog::CApplicationMonitor::Timeout_500ms);
}


char buf[200];

void loop(){
    unsigned long loop_start=micros();

    if(count_serial ==480){
        count_serial = 0;
        sprintf(buf,"y %d\r\np %d\r\nr %d\r\n",int_angle[0],int_angle[1],int_angle[2]);
        Serial1.print(buf);
    }else if(count_serial ==400){
        sprintf(buf,"gy %d\r\ngp %d\r\ngr %d\r\n",int_rate[0],int_rate[1],int_rate[2]);
        Serial1.print(buf);
        count_serial=count_serial+1;
    }else if(count_serial==320){
        sprintf(buf,"bs %d\r\nm1 %d\r\nm2 %d\r\n",base_speed,m1_speed,m2_speed);
        Serial1.print(buf);
        count_serial=count_serial+1;
    }else if(count_serial==240){
        sprintf(buf,"chp %d\r\nchr %d\r\nchy %d\r\n",ch2,ch4,ch1);
        Serial1.print(buf);
        count_serial=count_serial+1;
    }else if(count_serial==160){
        sprintf(buf,"ay %d\r\nap %d\r\nar %d\r\n",angle_pid_result[0],angle_pid_result[1],angle_pid_result[2]);
        Serial1.print(buf);
        count_serial=count_serial+1;
    }else if(count_serial==80){
        unsigned long cur_milli=millis();
        sprintf(buf,"cm %lu\r\nry %d\r\nrp %d\r\nrr %d\r\n",cur_milli,rate_pid_result[0],rate_pid_result[1],rate_pid_result[2]);
        Serial1.print(buf);
        count_serial=count_serial+1;
    }else{
        count_serial=count_serial+1;
    }

    if(count_check_serial & 0x40){
        check_serial();
    }else{
        count_check_serial=count_check_serial+1;
    }


    rc_update();
    ypr_update();
    pid_update();
    esc_update();
    ApplicationMonitor.IAmAlive();
}

void pid_init(){
    unsigned long yaw_tune_start=millis();
    while(millis()-yaw_tune_start<TIME_TILL_PROPER_YAW)
        ypr_update();

    int num_samples_for_yaw_average_copy=NUM_SAMPLES_FOR_YAW_AVERAGE;

    while(num_samples_for_yaw_average_copy>0){
        ypr_update();
        desired_yaw=desired_yaw*(1-YAW_AVERAGE_RETAIN)+YAW_AVERAGE_RETAIN*int_angle[0];
        num_samples_for_yaw_average_copy--;
    }

    desired_angle[0]=desired_yaw;
}


void ch1_change(){
    if(digitalRead(CH1_PIN) == HIGH){
        ch1_prev = micros();
    }else{
        ch1_val=micros()-ch1_prev;
        ch_changed=true;
        prev_ch_update=millis();
    }
}
void ch2_change(){
    if(digitalRead(CH2_PIN) == HIGH){
        ch2_prev = micros();
    }else{
        ch2_val=micros()-ch2_prev;
        ch_changed=true;
        prev_ch_update=millis();
    }
}
void ch3_change(){
    if(digitalRead(CH3_PIN) == HIGH){ 
        ch3_prev = micros();
    }else{
        ch3_val=micros()-ch3_prev;
        ch_changed=true;
        prev_ch_update=millis();
    }
}
void ch4_change(){
    if(digitalRead(CH4_PIN) == HIGH){ 
        ch4_prev = micros();
    }else{
        ch4_val=micros()-ch4_prev;
        ch_changed=true;
        prev_ch_update=millis();
    }
}
void ch5_change(){
    if(digitalRead(CH5_PIN) == HIGH){ 
        ch5_prev = micros();
    }else{
        ch5_val=micros()-ch5_prev;
        ch_changed=true;
        prev_ch_update=millis();
    }
}
void ch6_change(){
    if(digitalRead(CH6_PIN) == HIGH){ 
        ch6_prev = micros();
    }else{
        ch6_val=micros()-ch6_prev;
        ch_changed=true;
        prev_ch_update=millis();
    }
}

void mpu_init(){
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin(true);
        TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif


    // initialize device
    mpu.initialize();
    mpu.setRate(0);
    mpu.setFullScaleGyroRange(250);

    // verify connection
    if(!mpu.testConnection())
        Serial1.println(F("MPU connection failed"));

    // load and configure the DMP
    dev_status = mpu.dmpInitialize();

    // make sure it worked (returns 0 if so)
    if (dev_status == 0) {
        // turn on the DMP, now that it's ready
        /* Serial1.println(F("Enabling DMP...")); */
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        enableInterrupt(DMP_INT_PIN, dmp_data_ready, RISING);
        mpu_int_status = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        dmp_ready = true;

        // get expected DMP packet size for later comparison
        packet_size = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial1.print(F("DMP Initialization failed (code "));
        Serial1.print(dev_status);
        Serial1.println(F(")"));
		asm volatile ("  jmp 0");
    }

}

void esc_init(){
    m1.attach(ESC_1);
    m2.attach(ESC_2);
    m3.attach(ESC_3);
    m4.attach(ESC_4);
    delay(100);
    stop_motors();
    enable_pitch=true;
    enable_roll=true;
}

void stop_motors(){
	m1.writeMicroseconds(ESC_MIN);
	m2.writeMicroseconds(ESC_MIN);
	m3.writeMicroseconds(ESC_MIN);
	m4.writeMicroseconds(ESC_MIN);
}


void rc_init(){
    pinMode(CH1_PIN, INPUT);
    pinMode(CH2_PIN, INPUT);
    pinMode(CH3_PIN, INPUT);
    pinMode(CH4_PIN, INPUT);
    pinMode(CH5_PIN, INPUT);
    pinMode(CH6_PIN, INPUT);

    prev_ch_update=millis();

    enableInterrupt(CH1_PIN, ch1_change,CHANGE);
    enableInterrupt(CH2_PIN, ch2_change,CHANGE);
    enableInterrupt(CH3_PIN, ch3_change,CHANGE);
    enableInterrupt(CH4_PIN, ch4_change,CHANGE);
    enableInterrupt(CH5_PIN, ch5_change,CHANGE);
    enableInterrupt(CH6_PIN, ch6_change,CHANGE);
}

void ypr_update(){

	// wait for MPU interrupt or extra packet(s) available
	// reset interrupt flag and get INT_STATUS byte
    if(mpu_interrupt){
        mpu_interrupt = false;
        mpu_int_status = mpu.getIntStatus();

        // get current FIFO count
        fifo_count = mpu.getFIFOCount();
        /* loop_leave=fifo_count; */

        // check for overflow (this should never happen unless our code is too inefficient)
        if ((mpu_int_status & 0x10) || fifo_count == 1024) {
            // reset so we can continue cleanly
            Serial1.println(fifo_count);
            mpu.resetFIFO();
            Serial1.println(F("FIFO overflow!"));

            // otherwise, check for DMP data ready interrupt (this should happen frequently)
        }else if (mpu_int_status & 0x02) {
            // wait for correct available data length, should be a VERY short wait

            if (fifo_count >= packet_size) {

                // fifo_count = mpu.getFIFOCount();

                q_past = q;

                // read a packet from FIFO
                mpu.getFIFOBytes(fifo_buffer, packet_size);

                // track FIFO count here in case there is > 1 packet available
                // (this lets us immediately read more without waiting for an interrupt)
                fifo_count -= packet_size;
                while(fifo_count>packet_size){
                    mpu.getFIFOBytes(fifo_buffer, packet_size);
                    fifo_count -= packet_size;
                }
                // display Euler angles in degrees
                mpu.dmpGetQuaternion(&q, fifo_buffer);
                mpu.dmpGetGravity(&gravity, &q);
                mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
            }
        }
    }

    mpu.getRotation(&gx,&gy,&gz);
    gyro_int_raw[0]=gz*GYRO_RATIO+gyro_int_offset[0];
    // x and y are reversed to match pitch and roll.
    // pitch is about y axis and roll about x axis
    gyro_int_raw[1]=gy*GYRO_RATIO+gyro_int_offset[1];
    gyro_int_raw[2]=gx*GYRO_RATIO+gyro_int_offset[2];

    int_rate[0]=int_rate[0]*(1-gyro_retain[0])+gyro_retain[0]*gyro_int_raw[0];
    int_rate[1]=int_rate[1]*(1-gyro_retain[1])+gyro_retain[1]*gyro_int_raw[1];
    int_rate[2]=int_rate[2]*(1-gyro_retain[2])+gyro_retain[2]*gyro_int_raw[2];

    int_angle[0]=ypr[0]*YPR_RATIO+ypr_int_offset[0];
    int_angle[1]=ypr[1]*YPR_RATIO+ypr_int_offset[1];
    int_angle[2]=ypr[2]*YPR_RATIO+ypr_int_offset[2];
}


int angle_pid_constraint[3]={120,1000,1000};
int rate_pid_constraint[3]={8,50,50};
int angle_i_constraint[3]={0,40,40};
int rate_i_constraint[3]={0,4,4};
int rate_i_term_calc_interval=120;
int angle_i_term_calc_interval=120;
float angle_i_term[3]={0,0,0};
float rate_i_term[3]={0,0,0};
int angle_i_prev_calc_time=0;
int rate_i_prev_calc_time=0;
float angle_kp[3] = {1.0f, 24.0f, 24.0f}, angle_kd[3] = {0.0f, 0.0f, 0.0f}, angle_ki[3] = {0.0f,1.0f,1.0f};
float rate_kp[3]={0.0f,0.0625f,0.0625f}, rate_kd[3]={0.0f,0.0f,0.0f}, rate_ki[3]={0.0f,0.0f,0.0f};

inline void pid_update(){
    //PID direction
    //angle y-reverse,p-direct,r-reverse
    //rate y,p,r-reverse
    //Reverse means -ve kp,kd,ki
    //error is calculated using desired-actual
    if(millis()-angle_i_prev_calc_time>angle_i_term_calc_interval){
        angle_i_term[0]+=-angle_ki[0]*(desired_angle[0]-int_angle[0]);
        angle_i_term[1]+=angle_ki[1]*(desired_angle[1]-int_angle[1]);
        angle_i_term[2]+=-angle_ki[2]*(desired_angle[2]-int_angle[2]);

        angle_i_term[0]=constrain(angle_i_term[0],-angle_i_constraint[0],angle_i_constraint[0]);
        angle_i_term[1]=constrain(angle_i_term[1],-angle_i_constraint[1],angle_i_constraint[1]);
        angle_i_term[2]=constrain(angle_i_term[2],-angle_i_constraint[2],angle_i_constraint[2]);

        angle_i_prev_calc_time=millis();
    }

    angle_pid_result[0]=-angle_kp[0]*(desired_angle[0]-int_angle[0]) + angle_i_term[0];
    angle_pid_result[1]=angle_kp[1]*(desired_angle[1]-int_angle[1]) + angle_i_term[1];
    angle_pid_result[2]=-angle_kp[2]*(desired_angle[2]-int_angle[2]) + angle_i_term[2];

    angle_pid_result[0]=constrain(angle_pid_result[0],-angle_pid_constraint[0],angle_pid_constraint[0]);
    angle_pid_result[1]=constrain(angle_pid_result[1],-angle_pid_constraint[1],angle_pid_constraint[1]);
    angle_pid_result[2]=constrain(angle_pid_result[2],-angle_pid_constraint[2],angle_pid_constraint[2]);

    if(millis()-rate_i_prev_calc_time>rate_i_term_calc_interval){
        rate_i_term[0]+=-rate_ki[0]*(angle_pid_result[0]-int_rate[0]);
        rate_i_term[1]+=-rate_ki[1]*(angle_pid_result[1]-int_rate[1]);
        rate_i_term[2]+=-rate_ki[2]*(angle_pid_result[2]-int_rate[2]);

        rate_i_term[0]=constrain(rate_i_term[0],-rate_i_constraint[0],rate_i_constraint[0]);
        rate_i_term[1]=constrain(rate_i_term[1],-rate_i_constraint[1],rate_i_constraint[1]);
        rate_i_term[2]=constrain(rate_i_term[2],-rate_i_constraint[2],rate_i_constraint[2]);

        rate_i_prev_calc_time=millis();
    }

    rate_pid_result[0]=-rate_kp[0]*(angle_pid_result[0]-int_rate[0]) + rate_i_term[0];
    rate_pid_result[1]=-rate_kp[1]*(angle_pid_result[1]-int_rate[1]) + rate_i_term[1];
    rate_pid_result[2]=-rate_kp[2]*(angle_pid_result[2]-int_rate[2]) + rate_i_term[2];

    rate_pid_result[0]=constrain(rate_pid_result[0],-rate_pid_constraint[0],rate_pid_constraint[0]);
    rate_pid_result[1]=constrain(rate_pid_result[1],-rate_pid_constraint[1],rate_pid_constraint[1]);
    rate_pid_result[2]=constrain(rate_pid_result[2],-rate_pid_constraint[2],rate_pid_constraint[2]);

    rate_pid_result[0]=angle_pid_result[0];//not using rate pid for yaw axis
}


inline void esc_update()
{
	motor_enter = micros();

	// based on pitch
    m1_speed = base_speed - rate_pid_result[0] - rate_pid_result[1]+ m1_speed_off;
	m3_speed = base_speed -rate_pid_result[0] + rate_pid_result[1]+ m3_speed_off;

	// based on roll
	m2_speed = base_speed  +rate_pid_result[0] - rate_pid_result[2]+ m2_speed_off;
	m4_speed = base_speed  +rate_pid_result[0] + rate_pid_result[2]+ m4_speed_off;
	
	//constrain to to the pulse width limit we can give to the motor
	m1_speed = constrain(m1_speed, min_speed, max_speed);
	m2_speed = constrain(m2_speed, min_speed, max_speed);
	m3_speed = constrain(m3_speed, min_speed, max_speed);
	m4_speed = constrain(m4_speed, min_speed, max_speed);



	if(enable_motors && enable_pitch){
		m1.writeMicroseconds(m1_speed);
		m3.writeMicroseconds(m3_speed);
	}else{
		m1.writeMicroseconds(1000);
		m3.writeMicroseconds(1000);
	}
	
	if(enable_motors && enable_roll){
		m4.writeMicroseconds(m4_speed);
		m2.writeMicroseconds(m2_speed);
	}else{
		m4.writeMicroseconds(1000);
		m2.writeMicroseconds(1000);
	}

	motor_ratio = 0.5*motor_ratio + 0.5*(micros() - motor_enter)/(micros() - motor_leave);
	motor_leave = micros();

}


void rc_update(){
    boolean receiver_lost=millis()-prev_ch_update_copy>receiver_lost_threashold;
    if(ch_changed || receiver_lost){
        ch_changed=false;
        sregRestore = SREG;
        cli(); // clear the global interrupt enable flag
        ch1=ch1_val;
        ch2=ch2_val;
        ch3=ch3_val;
        ch4=ch4_val;
        ch5=ch5_val;
        ch6=ch6_val;
        prev_ch_update_copy=prev_ch_update;
        SREG = sregRestore; // restore the status register to its previous value

        ch1 = (ch1==0)?(CH1_MAX+CH1_MIN)/2:ch1;
        ch2 = (ch2==0)?(CH2_MAX+CH2_MIN)/2:ch2;
        ch3 = (ch3==0)?CH3_MIN_EFFECT:ch3;
        ch4 = (ch4==0)?(CH4_MAX+CH4_MIN)/2:ch4;
        ch5 = (ch5==0)?(CH5_MAX+CH5_MIN)/2:ch5;
        ch6 = (ch6==0)?(CH6_MAX+CH6_MIN)/2:ch6;


        ch1=constrain(ch1,CH1_MIN,CH1_MAX);
        ch2=constrain(ch2,CH2_MIN,CH2_MAX);
        ch3=constrain(ch3,CH3_MIN,CH3_MAX);
        ch4=constrain(ch4,CH4_MIN,CH4_MAX);
        ch5=constrain(ch5,CH5_MIN,CH5_MAX);
        ch6=constrain(ch6,CH6_MIN,CH6_MAX);

        ch1=map(ch1,CH1_MIN,CH1_MAX,-CH1_EFFECT,CH1_EFFECT);
        ch2=map(ch2,CH2_MIN,CH2_MAX,-CH2_EFFECT,CH2_EFFECT);
        ch3=map(ch3,CH3_MIN,CH3_MAX,CH3_MIN_EFFECT,CH3_MAX_EFFECT);
        ch4=map(ch4,CH4_MIN,CH4_MAX,-CH4_EFFECT,CH4_EFFECT);
        ch5=map(ch5,CH5_MIN,CH5_MAX,-CH5_EFFECT,CH5_EFFECT);
        ch6=map(ch6,CH6_MIN,CH6_MAX,-CH6_EFFECT,CH6_EFFECT);

        if(receiver_lost){
            //landing
            ch1=0;
            ch2=0;
            ch4=0;
            ch5=CH5_EFFECT;
        }

        desired_angle[0]=desired_yaw+ch1;
        desired_angle[1]=ch2;
        desired_angle[2]=ch4;


        if(ch6>0){
            if(ch5>CH5_EFFECT/2){
                if(millis()-take_down_start>=take_down_gradient){
                    take_down_start=millis();
                    if(base_speed>take_down_cutoff){
                        base_speed--;
                    }else{
                        base_speed=ESC_MIN;
                        enable_motors=false;
                    }
                }
            }else if(ch5<-CH5_EFFECT/2){
                //for startup
                angle_i_term[0]=0;
                angle_i_term[1]=0;
                angle_i_term[2]=0;
                rate_i_term[0]=0;
                rate_i_term[1]=0;
                rate_i_term[2]=0;
                base_speed=esc_problem_threashold;
                enable_motors=true;
            }else{
                enable_motors=true;
                take_down_count=0;
                take_off_count=0;
                base_speed=ch3;
                take_down_start=millis();
            }
        }else{
            enable_motors=false;
            //SO the error do not accumulate while sitting
            angle_i_term[0]=0;
            angle_i_term[1]=0;
            angle_i_term[2]=0;
            rate_i_term[0]=0;
            rate_i_term[1]=0;
            rate_i_term[2]=0;
        }
    }

}

inline void check_serial(){
	serial_send = "";

	if (Serial1.available() > 0){
		// read the value
		char ch = Serial1.read();

		if(ch != ';' && ch != '='){
			// Add the character to the in_str
			in_str += ch;
		}else if(ch == '='){
			in_key = in_str;
			in_str = "";
		}else if(ch == ';'){
			in_value = in_str;
			// print the incoming string
			serial_send += "Key: ";
			serial_send += in_key;
			serial_send += "\t\t";
			serial_send += "Value: ";
			serial_send += in_value;
			Serial.println(serial_send);//to terminal
			serial_send = "";

			// Convert the string to an integer
			float val = in_value.toFloat();

            if(in_key=="a_c"){
                angle_pid_constraint[1]=angle_pid_constraint[2]=val;
            }else if(in_key=="y_a_c"){
                angle_pid_constraint[0]=val;
            }else if(in_key=="r_c"){
                rate_pid_constraint[1]=rate_pid_constraint[2]=val;
            }else if(in_key=="y_r_c"){
                rate_pid_constraint[0]=val;
            }else if(in_key=="a_i_c"){
                angle_i_constraint[1]=angle_i_constraint[2]=val;
            }else if(in_key=="y_a_i_c"){
                angle_i_constraint[0]=val;
            }else if(in_key=="r_i_c"){
                rate_i_constraint[1]=rate_i_constraint[2]=val;
            }else if(in_key=="y_r_i_c"){
                rate_i_constraint[0]=val;
            }else if(in_key=="ar_m"){
                angle_i_term_calc_interval=val;
                rate_i_term_calc_interval=val;
            }else if(in_key=="pe"){
                CH2_EFFECT=val;
                /* Serial.println(CH2_EFFECT); */
            }else if(in_key=="re"){
                CH4_EFFECT=val;
                /* Serial.println(CH4_EFFECT); */
            }else if(in_key=="ye"){
                CH1_EFFECT=val;
                /* Serial.println(CH1_EFFECT); */
            }else if(in_key=="tl"){
                CH3_MIN_EFFECT=val;
                /* Serial.println(CH3_MIN_EFFECT); */
            }else if(in_key=="th"){
                CH3_MAX_EFFECT=val;
                /* Serial.println(CH3_MAX_EFFECT); */
            }else if(in_key == "r_kd"){
                rate_kd[1] = rate_kd[2] = val;
                /* Serial.println(rate_kd[1]); */
			}else if(in_key == "r_kp"){
                rate_kp[1] = rate_kp[2] = val;
                /* Serial.println(rate_kp[1]); */
			}else if(in_key == "r_ki"){
                rate_ki[1] = rate_ki[2] = val;
                //Should reset the previously accumulated error
                //cause they are calculated with a differnet ki
                angle_i_term[1]=0;
                angle_i_term[2]=0;
                rate_i_term[1]=0;
                rate_i_term[2]=0;
                /* Serial.println(rate_ki[1]); */
            }else if(in_key == "y_r_kd"){
                rate_kd[0]= val;
                /* Serial.println(rate_kd[0]); */
			}else if(in_key == "y_r_kp"){
                rate_kp[0]= val;
                /* Serial.println(rate_kp[0]); */
			}else if(in_key == "y_r_ki"){
                rate_ki[0] = val;
                angle_i_term[0]=0;
                rate_i_term[0]=0;
                /* Serial.println(rate_ki[0]); */
            }else if(in_key == "a_kd"){
                angle_kd[1] = angle_kd[2] = val;
                /* Serial.println(angle_kd[1]); */
			}else if(in_key == "a_kp"){
                angle_kp[1] = angle_kp[2] = val;
                /* Serial.println(angle_kp[1]); */
			}else if(in_key == "a_ki"){
                angle_ki[1] = angle_ki[2] = val;
                angle_i_term[1]=0;
                angle_i_term[2]=0;
                rate_i_term[1]=0;
                rate_i_term[2]=0;
                /* Serial.println(angle_ki[1]); */
            }else if(in_key == "y_a_kd"){
                angle_kd[0]= val;
                /* Serial.println(angle_kd[0]); */
			}else if(in_key == "y_a_kp"){
                angle_kp[0]= val;
                /* Serial.println(angle_kp[0]); */
			}else if(in_key == "y_a_ki"){
                angle_ki[0] = val;
                angle_i_term[0]=0;
                rate_i_term[0]=0;
                /* Serial.println(angle_ki[0]); */
			}else{
				serial_send += "Error with the input ";
				serial_send += in_key;
                //It will print on terminal
				Serial.println(serial_send);
			}
			in_key = in_value = in_str = in_index = "";
		}
	}
}


								/* 
								     up [*] -ve pitch

								     M 1

								     ||
								     ||
								  sparkfun
								     ||
down [X] +ve roll    M 2 ============||============  M 4     up [*] -ve roll
								     ||
								     ||
								     ||
								     ||

								     M 3

								   down [X] +ve pitch



                               _______\     
                                      /
                               (  yaw  )     clockwise +ve
                               /______
                               \


   roll values


   1500  27.00  25.00  5.00

 */

