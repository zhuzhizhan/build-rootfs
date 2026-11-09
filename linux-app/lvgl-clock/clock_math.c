#include "clock_math.h"

#include <math.h>

#define CLOCK_PI 3.14159265358979323846

void clock_math_angles(int hour, int minute, int second, long nanosecond, clock_angles_t *angles)
{
    double sec = (double)second + (double)nanosecond / 1e9;
    double min = (double)minute + sec / 60.0;
    double hr = (double)(hour % 12) + min / 60.0;

    angles->second = (float)(sec * 6.0);
    angles->minute = (float)(min * 6.0);
    angles->hour = (float)(hr * 30.0);
}

void clock_math_endpoint(float center_x, float center_y, float angle_deg, float length, float *x, float *y)
{
    double rad = (double)angle_deg * CLOCK_PI / 180.0;

    *x = center_x + (float)(sin(rad) * (double)length);
    *y = center_y - (float)(cos(rad) * (double)length);
}
