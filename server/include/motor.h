#ifndef UDP_SERVER_MOTOR_H
#define UDP_SERVER_MOTOR_H

#define RightMotorPin1       0
#define RightMotorPin2       2
#define RightMotorEnable     3

void *moveMotorRight(void *vargp);
void *moveMotorLeft(void *vargp);
void *stopMotor(void *vargp);

#endif //UDP_SERVER_MOTOR_H
