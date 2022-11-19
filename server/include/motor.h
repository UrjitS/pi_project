#ifndef UDP_SERVER_MOTOR_H
#define UDP_SERVER_MOTOR_H

#define MotorPin1       0
#define MotorPin2       2
#define MotorEnable     3

void *moveMotorRight(void *vargp);
void *moveMotorLeft(void *vargp);
void *stopMotor(void *vargp);

#endif //UDP_SERVER_MOTOR_H
