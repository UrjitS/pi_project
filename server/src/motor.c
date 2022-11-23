#include "../include/motor.h"
#include <stdio.h>
#include <wiringPi.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

int mSleep(long msec);

//https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
int mSleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}


void *moveMotorRight(void *vargp)
{
    printf("Clockwise\n");
    digitalWrite(RightMotorEnable, HIGH);
    digitalWrite(RightMotorPin1, HIGH);
    digitalWrite(RightMotorPin2, LOW);

    digitalWrite(LeftMotorEnable, HIGH);
    digitalWrite(LeftMotorPin1, HIGH);
    digitalWrite(LeftMotorPin2, LOW);
    mSleep(10);
    return NULL;
}

void *moveMotorLeft(void *vargp)
{
    printf("Anti-clockwise\n");
    digitalWrite(RightMotorEnable, HIGH);
    digitalWrite(RightMotorPin1, LOW);
    digitalWrite(RightMotorPin2, HIGH);

    digitalWrite(LeftMotorEnable, HIGH);
    digitalWrite(LeftMotorPin1, LOW);
    digitalWrite(LeftMotorPin2, HIGH);
    mSleep(10);
    return NULL;
}

void *stopMotor(void *vargp)
{
    printf("Stopping\n");
    digitalWrite(RightMotorEnable, LOW);
    digitalWrite(LeftMotorEnable, LOW);
    return NULL;
}
