#ifndef TIMER_H
#define TIMER_H


#include <SFML/System.hpp>

class Timer {
public:
    Timer() : m_running(false) {}

    void start() {
        if (!m_running) {
            m_clock.restart();
            m_running = true;
        }
    }

    void pause() {
        if (m_running) {
            m_elapsed += m_clock.getElapsedTime();
            m_running = false;
        }
    }

    void restart() {
        m_clock.restart();
        m_elapsed = sf::Time::Zero;
        m_running = true;
    }

    sf::Time getElapsedTime() const {
        if (m_running)
            return m_elapsed + m_clock.getElapsedTime();
        else
            return m_elapsed;
    }

    bool isRunning() const { return m_running; }

private:
    sf::Clock m_clock;
    sf::Time  m_elapsed;
    bool      m_running;
};

#endif // TIMER_H
