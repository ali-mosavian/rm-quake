/* fov.c -- Quake's horizontal cvar at qgl's vertical projection boundary. */

#include <math.h>

#include "fov.h"

#define FOV_PI 3.14159265

float fov_vertical( float horizontal, float aspect )
{
    double half = horizontal * FOV_PI / 360.0;
    return (float) ( 2.0 * atan( tan( half ) / aspect ) * 180.0 / FOV_PI );
}

float fov_horizontal( float vertical, float aspect )
{
    double half = vertical * FOV_PI / 360.0;
    return (float) ( 2.0 * atan( tan( half ) * aspect ) * 180.0 / FOV_PI );
}
