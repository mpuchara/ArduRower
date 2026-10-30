#pragma once
#include <stdint.h>
struct TS_Point {
  int16_t x=0,y=0,z=0;
  TS_Point()=default;
  TS_Point(int16_t xx,int16_t yy,int16_t zz):x(xx),y(yy),z(zz){}
};
