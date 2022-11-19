#include "../include/motor.h"
#include <stdio.h>
#include <wiringPi.h>
#include <unistd.h>

void *moveMotorRight(void *vargp)
{
    printf("Clockwise\n");
    digitalWrite(MotorEnable, HIGH);
    digitalWrite(MotorPin1, HIGH);
    digitalWrite(MotorPin2, LOW);
    sleep(1);
    return NULL;
}

void *moveMotorLeft(void *vargp)
{
    printf("Anti-clockwise\n");
    digitalWrite(MotorEnable, HIGH);
    digitalWrite(MotorPin1, LOW);
    digitalWrite(MotorPin2, HIGH);
    sleep(1);
    return NULL;
}

void *stopMotor(void *vargp)
{
    printf("Stopping\n");
    digitalWrite(MotorEnable, LOW);
    return NULL;
}
