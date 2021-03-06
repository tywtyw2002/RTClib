// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

#include "brzo_i2c.h"
#include "RTClib.h"
#include <pgmspace.h>

uint16_t I2C_SCL_FREQUENCY;
uint8_t buffer[8];

static uint8_t read_i2c_register(uint8_t addr, uint8_t reg) {
  //Wire.beginTransmission(addr);
  //Wire._I2C_WRITE((byte)reg);
  //Wire.endTransmission();
  uint8_t bcode = 0;
  uint8_t data;

  brzo_i2c_start_transaction(addr, I2C_SCL_FREQUENCY);
  buffer[0] = reg;
  brzo_i2c_write(buffer, 1, false);
  bcode = brzo_i2c_end_transaction();

  delay(50);

  if (bcode == 0) {
        brzo_i2c_start_transaction(addr, I2C_SCL_FREQUENCY);
        brzo_i2c_read(data, 1, false);
        bcode = brzo_i2c_end_transaction();
  }

  //Wire.requestFrom(addr, (byte)1);
  //return Wire._I2C_READ();
  return data;
}

static void write_i2c_register(uint8_t addr, uint8_t reg, uint8_t val) {
  brzo_i2c_start_transaction(addr, I2C_SCL_FREQUENCY);
  buffer[0] = reg;
  buffer[1] = val;
  brzo_i2c_write(buffer, 2, false);
  brzo_i2c_end_transaction();
  //Wire.beginTransmission(addr);
  //Wire._I2C_WRITE((byte)reg);
  //Wire._I2C_WRITE((byte)val);
  //Wire.endTransmission();
}


////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

const uint8_t daysInMonth [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += pgm_read_byte(daysInMonth + i - 1);
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000;    // bring to 2000 timestamp from 1970

    ss = t % 60;
    t /= 60;
    mm = t % 60;
    t /= 60;
    hh = t % 24;
    uint16_t days = t / 24;
    uint8_t leap;
    for (yOff = 0; ; ++yOff) {
        leap = yOff % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }
    for (m = 1; ; ++m) {
        uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = day;
    hh = hour;
    mm = min;
    ss = sec;
}

DateTime::DateTime (const DateTime& copy):
  yOff(copy.yOff),
  m(copy.m),
  d(copy.d),
  hh(copy.hh),
  mm(copy.mm),
  ss(copy.ss)
{}

static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using F() would further reduce the RAM footprint, see below.
DateTime::DateTime (const char* date, const char* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    yOff = conv2d(date + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (date[0]) {
        case 'J': m = (date[1] == 'a') ? 1 : ((date[2] == 'n') ? 6 : 7); break;
        case 'F': m = 2; break;
        case 'A': m = date[2] == 'r' ? 4 : 8; break;
        case 'M': m = date[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(date + 4);
    hh = conv2d(time);
    mm = conv2d(time + 3);
    ss = conv2d(time + 6);
}

// A convenient constructor for using "the compiler's time":
// This version will save RAM by using PROGMEM to store it by using the F macro.
//   DateTime now (F(__DATE__), F(__TIME__));
DateTime::DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    char buff[11];
    memcpy_P(buff, date, 11);
    yOff = conv2d(buff + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (buff[0]) {
        case 'J': m = (buff[1] == 'a') ? 1 : ((buff[2] == 'n') ? 6 : 7); break;
        case 'F': m = 2; break;
        case 'A': m = buff[2] == 'r' ? 4 : 8; break;
        case 'M': m = buff[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(buff + 4);
    memcpy_P(buff, time, 8);
    hh = conv2d(buff);
    mm = conv2d(buff + 3);
    ss = conv2d(buff + 6);
}

uint8_t DateTime::dayOfTheWeek() const {
    uint16_t day = date2days(yOff, m, d);
    return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::unixtime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2long(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000;  // seconds from 1970 to 2000

  return t;
}

long DateTime::secondstime(void) const {
  long t;
  uint16_t days = date2days(yOff, m, d);
  t = time2long(days, hh, mm, ss);
  return t;
}

DateTime DateTime::operator+(const TimeSpan& span) {
  return DateTime(unixtime()+span.totalseconds());
}

DateTime DateTime::operator-(const TimeSpan& span) {
  return DateTime(unixtime()-span.totalseconds());
}

TimeSpan DateTime::operator-(const DateTime& right) {
  return TimeSpan(unixtime()-right.unixtime());
}

////////////////////////////////////////////////////////////////////////////////
// TimeSpan implementation

TimeSpan::TimeSpan (int32_t seconds):
  _seconds(seconds)
{}

TimeSpan::TimeSpan (int16_t days, int8_t hours, int8_t minutes, int8_t seconds):
  _seconds((int32_t)days*86400L + (int32_t)hours*3600 + (int32_t)minutes*60 + seconds)
{}

TimeSpan::TimeSpan (const TimeSpan& copy):
  _seconds(copy._seconds)
{}

TimeSpan TimeSpan::operator+(const TimeSpan& right) {
  return TimeSpan(_seconds+right._seconds);
}

TimeSpan TimeSpan::operator-(const TimeSpan& right) {
  return TimeSpan(_seconds-right._seconds);
}

////////////////////////////////////////////////////////////////////////////////
// RTC_DS3231 implementation
static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

boolean RTC_DS3231::begin(void) {
  //Wire.begin();
  return true;
}

bool RTC_DS3231::lostPower(void) {
  return (read_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG) >> 7);
}

void RTC_DS3231::setSpeed(uint16_t speed) {
    I2C_SCL_FREQUENCY = speed;
}

void RTC_DS3231::adjust(const DateTime& dt) {
  //Wire.beginTransmission(DS3231_ADDRESS);
  brzo_i2c_start_transaction(DS3231_ADDRESS, I2C_SCL_FREQUENCY);
  buffer[0] = 0;
  buffer[1] = bin2bcd(dt.second());
  buffer[2] = bin2bcd(dt.minute());
  buffer[3] = bin2bcd(dt.hour());

  //Wire._I2C_WRITE((byte)0); // start at location 0
  //Wire._I2C_WRITE(bin2bcd(dt.second()));
  //Wire._I2C_WRITE(bin2bcd(dt.minute()));
  //Wire._I2C_WRITE(bin2bcd(dt.hour()));

  buffer[4] = 0;
  buffer[5] = bin2bcd(dt.day());
  buffer[6] = bin2bcd(dt.month());
  buffer[7] = bin2bcd(dt.year() - 2000);
  brzo_i2c_write(buffer, 8, false);

  //Wire._I2C_WRITE(bin2bcd(0));
  //Wire._I2C_WRITE(bin2bcd(dt.day()));
  //Wire._I2C_WRITE(bin2bcd(dt.month()));
  //Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
  //Wire.endTransmission();
  brzo_i2c_end_transaction();

  uint8_t statreg = read_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG);
  statreg &= ~0x80; // flip OSF bit
  write_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG, statreg);
}

DateTime RTC_DS3231::now() {
  //Wire.beginTransmission(DS3231_ADDRESS);
  uint8_t	bcode = 0;

  brzo_i2c_start_transaction(DS3231_ADDRESS, I2C_SCL_FREQUENCY);
  buffer[0] = 0;
  brzo_i2c_write(buffer, 1, false);
  bcode = brzo_i2c_end_transaction();
  //Wire._I2C_WRITE((byte)0);
  //Wire.endTransmission();
  delay(50);
  //Wire.requestFrom(DS3231_ADDRESS, 7);
  if (bcode == 0) {
      brzo_i2c_start_transaction(DS3231_ADDRESS, I2C_SCL_FREQUENCY);
      brzo_i2c_read(buffer, 7, false);
      bcode = brzo_i2c_end_transaction();
  }

  //uint8_t ss = bcd2bin(Wire._I2C_READ() & 0x7F);
  uint8_t ss = bcd2bin(buffer[0] & 0x7F);
  //uint8_t mm = bcd2bin(Wire._I2C_READ());
  uint8_t mm = bcd2bin(buffer[1]);
  //uint8_t hh = bcd2bin(Wire._I2C_READ());
  uint8_t hh = bcd2bin(buffer[2]);
  //Wire._I2C_READ();
  //uint8_t d = bcd2bin(Wire._I2C_READ());
  uint8_t d = bcd2bin(buffer[4]);
  //uint8_t m = bcd2bin(Wire._I2C_READ());
  uint8_t m = bcd2bin(buffer[5]);
  //uint16_t y = bcd2bin(Wire._I2C_READ()) + 2000;
  uint16_t y = bcd2bin(buffer[6]) + 2000;

  return DateTime (y, m, d, hh, mm, ss);
}

Ds3231SqwPinMode RTC_DS3231::readSqwPinMode() {
  int bcode = 0;
  int mode;

  brzo_i2c_start_transaction(DS3231_ADDRESS, I2C_SCL_FREQUENCY);
  buffer[0] = DS3231_CONTROL;
  brzo_i2c_write(buffer, 1, false);
  bcode = brzo_i2c_end_transaction();

  //Wire.beginTransmission(DS3231_ADDRESS);
  //Wire._I2C_WRITE(DS3231_CONTROL);
  //Wire.endTransmission();

  if (bcode == 0) {
      brzo_i2c_start_transaction(DS3231_ADDRESS, I2C_SCL_FREQUENCY);
      brzo_i2c_read(buffer, 1, false);
      brzo_i2c_end_transaction();
  }
  //Wire.requestFrom((uint8_t)DS3231_ADDRESS, (uint8_t)1);
  //mode = Wire._I2C_READ();
  mode = buffer[0];
  mode &= 0x93;
  return static_cast<Ds3231SqwPinMode>(mode);
}

void RTC_DS3231::writeSqwPinMode(Ds3231SqwPinMode mode) {
  uint8_t ctrl;
  ctrl = read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL);

  ctrl &= ~0x04; // turn off INTCON
  ctrl &= ~0x18; // set freq bits to 0

  if (mode == DS3231_OFF) {
    ctrl |= 0x04; // turn on INTCN
  } else {
    ctrl |= mode;
  }
  write_i2c_register(DS3231_ADDRESS, DS3231_CONTROL, ctrl);

  //Serial.println( read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL), HEX);
}
