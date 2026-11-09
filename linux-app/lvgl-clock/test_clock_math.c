#include "clock_math.h"

#include <math.h>
#include <stdio.h>

static int failures;

static void expect_near(const char *name, float got, float want, float tol)
{
    if(fabsf(got - want) > tol) {
        printf("FAIL %-24s got %.6f want %.6f\n", name, got, want);
        failures++;
    }
}

static void expect_point(const char *name, float x, float y, float want_x, float want_y)
{
    if(fabsf(x - want_x) > 0.001f || fabsf(y - want_y) > 0.001f) {
        printf("FAIL %-24s got (%.3f, %.3f) want (%.3f, %.3f)\n", name, x, y, want_x, want_y);
        failures++;
    }
}

int main(void)
{
    clock_angles_t a;

    clock_math_angles(0, 0, 0, 0, &a);
    expect_near("00:00:00 hour", a.hour, 0.0f, 0.001f);
    expect_near("00:00:00 minute", a.minute, 0.0f, 0.001f);
    expect_near("00:00:00 second", a.second, 0.0f, 0.001f);

    clock_math_angles(3, 0, 0, 0, &a);
    expect_near("03:00:00 hour", a.hour, 90.0f, 0.001f);
    expect_near("03:00:00 minute", a.minute, 0.0f, 0.001f);

    clock_math_angles(6, 30, 0, 0, &a);
    expect_near("06:30:00 hour", a.hour, 195.0f, 0.001f);
    expect_near("06:30:00 minute", a.minute, 180.0f, 0.001f);

    clock_math_angles(12, 0, 0, 0, &a);
    expect_near("12:00:00 hour", a.hour, 0.0f, 0.001f);

    clock_math_angles(23, 59, 59, 500000000L, &a);
    expect_near("23:59:59.5 second", a.second, 357.0f, 0.001f);
    expect_near("23:59:59.5 minute", a.minute, 359.95f, 0.01f);
    expect_near("23:59:59.5 hour", a.hour, 359.99583f, 0.01f);

    clock_math_angles(3, 30, 0, 0, &a);
    expect_near("03:30:00 hour", a.hour, 105.0f, 0.001f);
    expect_near("03:30:00 minute", a.minute, 180.0f, 0.001f);

    float x, y;
    clock_math_endpoint(100.0f, 100.0f, 0.0f, 50.0f, &x, &y);
    expect_point("endpoint 0 deg", x, y, 100.0f, 50.0f);
    clock_math_endpoint(100.0f, 100.0f, 90.0f, 50.0f, &x, &y);
    expect_point("endpoint 90 deg", x, y, 150.0f, 100.0f);
    clock_math_endpoint(100.0f, 100.0f, 180.0f, 50.0f, &x, &y);
    expect_point("endpoint 180 deg", x, y, 100.0f, 150.0f);
    clock_math_endpoint(100.0f, 100.0f, 270.0f, 50.0f, &x, &y);
    expect_point("endpoint 270 deg", x, y, 50.0f, 100.0f);
    clock_math_endpoint(100.0f, 100.0f, 0.0f, -50.0f, &x, &y);
    expect_point("endpoint 0 deg tail", x, y, 100.0f, 150.0f);
    clock_math_endpoint(100.0f, 100.0f, 90.0f, -50.0f, &x, &y);
    expect_point("endpoint 90 deg tail", x, y, 50.0f, 100.0f);

    if(failures) {
        printf("FAIL clock_math (%d checks)\n", failures);
        return 1;
    }
    printf("PASS clock_math\n");
    return 0;
}
