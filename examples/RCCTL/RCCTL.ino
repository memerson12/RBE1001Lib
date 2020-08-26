#include <Arduino.h>
#include <RBE1001Lib.h>
#include "Motor.h"
#include "Rangefinder.h"
#include <ESP32Servo.h>
#include <ESP32AnalogRead.h>
#include <Esp32WifiManager.h>
#include "wifi/WifiManager.h"
#include "WebPage.h"
#include <Timer.h>

const char *strings[12] = { "Left Encoder Degrees","Left Effort","Left Encoder Degrees/sec",
		"Right Encoder Degrees","Right  Effort","Right Encoder Degrees/sec" ,
				"2 Encoder Degrees","2 Encoder Effort","2 Encoder Degrees-sec" ,
				"3 Encoder Degrees","3 Encoder Effort","3 Encoder Degrees-sec"
};

// https://wpiroboticsengineering.github.io/RBE1001Lib/classMotor.html
Motor left_motor;
Motor right_motor;
// https://wpiroboticsengineering.github.io/RBE1001Lib/classRangefinder.html
Rangefinder rangefinder1;
// https://wpiroboticsengineering.github.io/RBE1001Lib/classServo.html
Servo lifter;
// https://wpiroboticsengineering.github.io/RBE1001Lib/classESP32AnalogRead.html
ESP32AnalogRead leftLineSensor;
ESP32AnalogRead rightLineSensor;
ESP32AnalogRead servoPositionFeedback;

WebPage control_page;


WifiManager manager;


Timer dashboardUpdateTimer;  // times when the dashboard should update
/*
 * This is the standard setup function that is called when the ESP32 is rebooted
 * It is used to initialize anything that needs to be done once.
 * In this example, it sets the Serial Console speed, initializes the web server,
 * sets up some web page buttons, resets some timers, and sets the initial state
 * the robot should start in
 */
int inc=0;
void setup() {
	manager.setup();
	while (manager.getState() != Connected) {
		manager.loop();
		delay(1);
	}
	Motor::allocateTimer(0); // used by the DC Motors
	ESP32PWM::allocateTimer(1); // Used by servos
	control_page.initalize();
	// pin definitions https://wpiroboticsengineering.github.io/RBE1001Lib/RBE1001Lib_8h.html#define-members
	right_motor.attach(MOTOR_RIGHT_PWM, MOTOR_RIGHT_DIR, MOTOR_RIGHT_ENCA, MOTOR_RIGHT_ENCB);
	left_motor.attach(MOTOR_LEFT_PWM, MOTOR_LEFT_DIR, MOTOR_LEFT_ENCA, MOTOR_LEFT_ENCB);
	rangefinder1.attach(SIDE_ULTRASONIC_TRIG, SIDE_ULTRASONIC_ECHO);
	lifter.attach(SERVO_PIN);
	leftLineSensor.attach(LEFT_LINE_SENSE);
	rightLineSensor.attach(RIGHT_LINE_SENSE);
	servoPositionFeedback.attach(SERVO_FEEDBACK_SENSOR);
	lifter.write(0);
	//control_page.setValue("Simple Counter",
	//				inc++);
	dashboardUpdateTimer.reset(); // reset the dashbaord refresh timer



}

/*
 * this is the state machine.
 * You can add additional states as desired. The switch statement will execute
 * the correct code depending on the state the robot is in currently.
 * For states that require timing, like turning and straight, they use a timer
 * that is zeroed when the state begins. It is compared with the number of
 * milliseconds the robot should reamain in that state.
 */
void runStateMachine() {

	float left = (control_page.getJoystickX()+control_page.getJoystickY())*360;
	float right = (control_page.getJoystickX()-control_page.getJoystickY())*360;

	left_motor.SetSpeed(left);
	right_motor.SetSpeed(right);
	lifter.write(control_page.getSliderValue(0)*180);
}

/*
 * This function updates all the dashboard status that you would like to see
 * displayed periodically on the web page. You are free to add more values
 * to be displayed to help debug your robot program by calling the
 * "setValue" function with a name and a value.
 */

uint32_t packet_old=0;
void updateDashboard() {
	// This writes values to the dashboard area at the bottom of the web page
	if (dashboardUpdateTimer.getMS() > 100) {
		control_page.setValue("Left linetracker", leftLineSensor.readMiliVolts());
		control_page.setValue("Right linetracker",
				rightLineSensor.readMiliVolts());
		control_page.setValue("Ultrasonic",
				rangefinder1.getDistanceCM());
		control_page.setValue("packets from Web to ESP",
						control_page.rxPacketCount);
		control_page.setValue("packets to Web from ESP",
						control_page.txPacketCount);
		control_page.setValue("slider",
						control_page.getSliderValue(0)*100);
		for (int i = 0; i < MAX_POSSIBLE_MOTORS; i++) {
			if (Motor::list[i] != NULL) {
				control_page.setValue(strings[i*3],Motor::list[i]->getCurrentDegrees());
				control_page.setValue(strings[i*3+1],Motor::list[i]->GetEffort());
				control_page.setValue(strings[i*3+2],Motor::list[i]->getDegreesPerSecond());
			}
		}
		dashboardUpdateTimer.reset();
	}
}

/*
 * The main loop for the program. The loop function is repeatedly called
 * once the ESP32 is started. In here we run the state machine, update the
 * dashboard data, and handle any web server requests.
 */

void loop() {

	//control_page.setValue("Packets",control_page.packetCount);
	manager.loop();
	runStateMachine();  // do a pass through the state machine
	if(manager.getState() == Connected)// only update if WiFi is up
		updateDashboard();  // update the dashboard values
}
