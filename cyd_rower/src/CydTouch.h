#pragma once
#include <SPI.h>
#include <XPT2046_Touchscreen.h> // TS_Point only

// Explicit byte transfers avoid transfer16 byte-order differences between
// Arduino cores. Each conversion has its own command and complete 16 clocks.
// IRQ is diagnostic only: a stale IRQ must not freeze pressure/release reads.
class CydTouch {
 public:
  CydTouch(uint8_t cs,uint8_t irq):cs_(cs),irq_(irq) {}
  bool begin(SPIClass &spi) {
    spi_=&spi;
    pinMode(cs_,OUTPUT);digitalWrite(cs_,HIGH);pinMode(irq_,INPUT);
    return true; // bus is initialized with the CYD pins by setup()
  }
  void setRotation(uint8_t) {} // raw coordinates match XPT2046 rotation 0
  void useSoftwareSpi() {
    spi_->end();software_=true;
    pinMode(25,OUTPUT);pinMode(32,OUTPUT);pinMode(39,INPUT);
    digitalWrite(25,LOW);digitalWrite(cs_,HIGH);
  }
  int z1() const {return z1_;}
  int z2() const {return z2_;}
  bool touched() { update();return point_.z>=300; }
  TS_Point getPoint() { update();return point_; }
 private:
  uint16_t adc(uint8_t command) {
    transfer(command);
    const uint16_t hi=transfer(0);
    return ((hi<<8)|transfer(0))>>3;
  }
  uint8_t transfer(uint8_t value) {
    if(!software_)return spi_->transfer(value);
    uint8_t received=0;
    for(int bit=7;bit>=0;--bit) {
      digitalWrite(32,(value>>bit)&1);
      delayMicroseconds(1);digitalWrite(25,HIGH);
      delayMicroseconds(1);
      received=(received<<1)|(digitalRead(39)?1:0);
      digitalWrite(25,LOW);
    }
    return received;
  }
  static int median(int a,int b,int c) {
    if(a>b){int t=a;a=b;b=t;}if(b>c){int t=b;b=c;c=t;}
    return a>b?a:b;
  }
  void update() {
    const uint32_t now=millis();
    if(sampled_ && now-lastMs_<8)return;
    sampled_=true;lastMs_=now;
    if(!software_)spi_->beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));
    digitalWrite(cs_,LOW);
    const int z1=adc(0xB1),z2=adc(0xC1);
    z1_=z1;z2_=z2;
    const int pressure=z1+4095-z2;
    if(z1>0 && z2<4095 && pressure>=300) {
      int x[3],y[3];
      for(int i=0;i<3;++i){x[i]=adc(0xD1);y[i]=adc(0x91);}
      point_=TS_Point(4095-median(x[0],x[1],x[2]),
                      median(y[0],y[1],y[2]),pressure);
    } else point_.z=0;
    adc(0x90); // power down ADC, enable PENIRQ
    digitalWrite(cs_,HIGH);if(!software_)spi_->endTransaction();
  }
  SPIClass *spi_=nullptr;
  uint8_t cs_,irq_;
  bool sampled_=false;
  bool software_=false;
  int z1_=0,z2_=0;
  uint32_t lastMs_=0;
  TS_Point point_;
};
