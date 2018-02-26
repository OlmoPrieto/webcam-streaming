#ifndef __CHRONO_H__
#define __CHRONO_H__

#include <chrono>

class Chrono {
public: 
  Chrono() {
  }
  ~Chrono() {
  }
  void start() {
    m_cStart = m_cClock.now();
  }
  void stop() {
    m_cEnd = m_cClock.now();
    m_cTime = std::chrono::duration_cast<std::chrono::duration<float>>(m_cEnd - m_cStart);
  }
  float timeAsSeconds() const {
    return m_cTime.count();
  }
  float timeAsMilliseconds() const {
    return m_cTime.count() * 1000.0f;
  }
  std::chrono::duration<float> m_cTime;
  std::chrono::high_resolution_clock m_cClock;
  std::chrono::high_resolution_clock::time_point m_cStart;
  std::chrono::high_resolution_clock::time_point m_cEnd;
};

#endif // __CHRONO_H__