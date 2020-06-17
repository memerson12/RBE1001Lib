 #pragma once
//
//#define FORWARD_ULTRASONIC_TRIG 13
//#define FORWARD_ULTRASONIC_ECHO 12

#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK  18
#define SPI_SS   15

#define SIDE_ULTRASONIC_TRIG 14
#define SIDE_ULTRASONIC_ECHO 27

#define LEFT_LINE_SENSE 36
#define RIGHT_LINE_SENSE 39

#define SERVO_FEEDBACK_SENSOR		34
/**
 * Gripper pin for Servo
 */
#define SERVO_PIN 12

/**
 * Drive motor 1 10Khz full duty PWM pin
 */
#define MOTOR1_PWM 13
/**
 * Pin for setting the direction of the H-Bridge
 */
#define MOTOR1_DIR 4
/**
 * Drive motor 2 10Khz full duty PWM pin
 */
#define MOTOR2_PWM 15
/**
 * Pin for setting the direction of the H-Bridge
 */
#define MOTOR2_DIR 25
//Encoder pins
#define MOTOR1_ENCA 26
#define MOTOR1_ENCB 33

#define MOTOR2_ENCA 17
#define MOTOR2_ENCB 16

// Pins used by a perpheral, may be re-used
#define BOOT_FLAG_PIN 			0
#define I2C_SDA       			21
#define I2C_SCL       			22
#define SERIAL_PROGRAMMING_TX 	1
#define SERIAL_PROGRAMMING_RX 	3