#include "../include/motor.h"
#include <stdio.h>
#include <wiringPi.h>
#include <unistd.h>

void *moveMotorRight(void *vargp)
{
    printf("Clockwise\n");
    digitalWrite(RightMotorEnable, HIGH);
    digitalWrite(RightMotorPin1, HIGH);
    digitalWrite(RightMotorPin2, LOW);
    sleep(1);
    return NULL;
}

void *moveMotorLeft(void *vargp)
{
    printf("Anti-clockwise\n");
    digitalWrite(RightMotorEnable, HIGH);
    digitalWrite(RightMotorPin1, LOW);
    digitalWrite(RightMotorPin2, HIGH);
    sleep(1);
    return NULL;
}

void *stopMotor(void *vargp)
{
    printf("Stopping\n");
    digitalWrite(RightMotorEnable, LOW);
    return NULL;
}
