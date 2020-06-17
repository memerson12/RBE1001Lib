/*
 * Motor.cpp
 *
 *  Created on: May 31, 2020
 *      Author: hephaestus
 */

#include <Motor.h>
hw_timer_t *Motor::timer = NULL;

bool Motor::timersAllocated = false;
Motor * Motor::list[MAX_POSSIBLE_MOTORS] = { NULL, };
static TaskHandle_t complexHandlerTask;
portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

float myFmapBounded(float x, float in_min, float in_max, float out_min,
		float out_max) {
	if (x > in_max)
		return out_max;
	if (x < in_min)
		return out_min;
	return ((x - in_min) * (out_max - out_min) / (in_max - in_min)) + out_min;
}
/**
 * getInterpolationUnitIncrement A unit vector from
 * 0 to 1 representing the state of the interpolation.
 */
float Motor::getInterpolationUnitIncrement(){
	float interpElapsed = (float) (millis() - startTime);
	if(interpElapsed < duration && duration > 0){
		// linear elapsed duration
		unitDuration = interpElapsed / duration;
		if (mode == SINUSOIDAL_INTERPOLATION) {
			// sinusoidal ramp up and ramp down
			float sinPortion = (cos(-PI * unitDuration) / 2) + 0.5;
			unitDuration = 1 - sinPortion;
		}
		return unitDuration;
	}
	return 1;
}
void onMotorTimer(void *param) {
	Serial.println("Starting the PID loop thread");
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = 1;
	xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil( &xLastWakeTime, xFrequency );
		//
		for (int i = 0; i < MAX_POSSIBLE_MOTORS; i++) {
			if (Motor::list[i] != NULL) {

				Motor::list[i]->loop();

			}
		}
		//
	}
	Serial.println("ERROR Pid thread died!");

}
/**
 * @param PWMgenerationTimer the timer to be used to generate the 20khx PWM
 * @param controllerTimer a timer running at 1khz for PID, velocity measurment, and trajectory planning
 */
void Motor::allocateTimer(int PWMgenerationTimer) {
	if (!Motor::timersAllocated) {
		ESP32PWM::allocateTimer(PWMgenerationTimer);
		xTaskCreate(onMotorTimer, "PID loop Thread", 8192, NULL, 1,
				&complexHandlerTask);
	}
	Motor::timersAllocated = true;
}

Motor::Motor() {
	pwm = NULL;
	encoder = NULL;
}

Motor::~Motor() {
	encoder->pauseCount();
	pwm->detachPin(MotorPWMPin);
	delete (encoder);
	delete (pwm);
}

/**
 * SetSetpoint in degrees with time
 * Set the setpoint for the motor in degrees
 * @param newTargetInDegrees the new setpoint for the closed loop controller
 * @param miliseconds the number of miliseconds to get from current position to the new setpoint
 */
void Motor::SetSetpointWithTime(float newTargetInDegrees, long msTimeDuration,
		interpolateMode mode){
	float newSetpoint =newTargetInDegrees/TICKS_TO_DEGREES;
	portENTER_CRITICAL(&mmux);
	closedLoopControl=true;
	if(newSetpoint==Setpoint &&msTimeDuration== duration&& this->mode==mode)
		return;
	startSetpoint = Setpoint;
	endSetpoint = newSetpoint;
	startTime = millis();
	duration = msTimeDuration;
	this->mode = mode;
	if(msTimeDuration<1){
		Setpoint=newSetpoint;
	}
	portEXIT_CRITICAL(&mmux);
}

/**
 * Loop function
 * this method is called by the timer to run the PID control of the motors and ensure strict timing
 *
 */
void Motor::loop() {
	portENTER_CRITICAL(&mmux);
	nowEncoder = encoder->getCount();
	if(closedLoopControl){
		unitDuration=getInterpolationUnitIncrement();
		if (unitDuration<1) {
			float setpointDiff = endSetpoint - startSetpoint;
			float newSetpoint = startSetpoint + (setpointDiff * unitDuration);
			Setpoint = newSetpoint;
		} else {
			// If there is no interpoation to perform, set the setpoint to the end state
			Setpoint = endSetpoint;
		}

		float controlErr = Setpoint-nowEncoder;
		// shrink old values out of the sum
		runntingITerm=runntingITerm*((I_TERM_SIZE-1.0)/I_TERM_SIZE);
		// running sum of error
		runntingITerm+=controlErr;

		currentEffort=controlErr*kP+((runntingITerm/I_TERM_SIZE)*kI);
	}
	portEXIT_CRITICAL(&mmux);
	interruptCountForVelocity++;
	if (interruptCountForVelocity == 50) {
		interruptCountForVelocity = 0;
		float err = prevousCount - nowEncoder;
		cachedSpeed = err / (0.05); // ticks per second
		prevousCount = nowEncoder;
	}
	SetEffortLocal(currentEffort);

}
/**
 * PID gains for the PID controller
 */
void Motor::setGains(float p,float i,float d){
	portENTER_CRITICAL(&mmux);
	kP=p;
	kI=i;
	kD=d;
	runntingITerm=0;
	portEXIT_CRITICAL(&mmux);
}

/**
 * Attach the motors hardware
 * @param MotorPWMPin the pin that produce PWM at 20kHz (Max is 250khz per DRV8838 datasheet)
 * @param MotorDirectionPin motor direction setting pin
 * @param the A channel of the encoder
 * @param the B channel of the encoder
 * @note this must only be called after timers are allocated via Motor::allocateTimers
 *
 */
void Motor::attach(int MotorPWMPin, int MotorDirectionPin, int EncoderA,
		int EncoderB) {
	pwm = new ESP32PWM();
	encoder = new ESP32Encoder();
	ESP32Encoder::useInternalWeakPullResistors = UP;

	pwm->attachPin(MotorPWMPin, 20000, 8);
	directionFlag = MotorDirectionPin;
	this->MotorPWMPin=MotorPWMPin;
	encoder->attachHalfQuad(EncoderA, EncoderB);
	pinMode(directionFlag, OUTPUT);
	// add the motor to the list of timer based controls
	for (int i = 0; i < MAX_POSSIBLE_MOTORS; i++) {
		if (Motor::list[i] == NULL) {
			Motor::list[i] = this;
			Serial.println(
					"Allocating Motor " + String(i) + " on PWM "
							+ String(MotorPWMPin));
			return;
		}

	}
}

/*
 * effort of the motor
 * @param a value from -1 to 1 representing effort
 *        0 is brake
 *        1 is full speed clockwise
 *        -1 is full speed counter clockwise
 */
void Motor::SetEffort(float effort) {
	if (effort>1)
		effort=1;
	if(effort<-1)
		effort=-1;
	portENTER_CRITICAL(&mmux);
	closedLoopControl=false;
	currentEffort=effort;
	portEXIT_CRITICAL(&mmux);
}
/*
 * effort of the motor
 * @param a value from -1 to 1 representing effort
 *        0 is brake
 *        1 is full speed clockwise
 *        -1 is full speed counter clockwise
 */
void Motor::SetEffortLocal(float effort) {
	if (effort>1)
		effort=1;
	if(effort<-1)
		effort=-1;
	portENTER_CRITICAL(&mmux);
	if(effort>0)
		digitalWrite(directionFlag, HIGH);
	else
		digitalWrite(directionFlag, LOW);
	pwm->writeScaled(abs(effort));
	portEXIT_CRITICAL(&mmux);
}
/**
 * getDegreesPerSecond
 *
 * This function returns the current speed of the motor
 *
 * @return the speed of the motor in degrees per second
 */
float Motor::getDegreesPerSecond() {
	float tmp;
	portENTER_CRITICAL(&mmux);
	tmp = cachedSpeed;
	portEXIT_CRITICAL(&mmux);
	return tmp*TICKS_TO_DEGREES;
}
/**
 * getTicks
 *
 * This function returns the current count of encoders
 * @return count
 */
int Motor::getCurrentDegrees() {
	float tmp;
	portENTER_CRITICAL(&mmux);
	tmp = nowEncoder;
	portEXIT_CRITICAL(&mmux);
	return tmp*TICKS_TO_DEGREES;
}
