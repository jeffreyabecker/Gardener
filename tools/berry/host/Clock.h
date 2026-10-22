#pragma once

// The clock the firmware reads through millis(). On the device it is the core's; here it is the
// check's, which advances it a tick at a time so an hour of a page's run costs no real time.

namespace host {

unsigned long millis();
void advance(unsigned long ms);
void reset();

}  // namespace host

inline unsigned long millis() { return host::millis(); }
inline void delay(unsigned long ms) { host::advance(ms); }
