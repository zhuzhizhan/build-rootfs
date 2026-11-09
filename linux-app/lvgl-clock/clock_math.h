#ifndef CLOCK_MATH_H
#define CLOCK_MATH_H

typedef struct {
    float hour;
    float minute;
    float second;
} clock_angles_t;

void clock_math_angles(int hour, int minute, int second, long nanosecond, clock_angles_t *angles);
void clock_math_endpoint(float center_x, float center_y, float angle_deg, float length, float *x, float *y);

#endif
