#pragma once

#include <chrono>
#include <cstdio>
#include <string>

class Timer
{
public:
    Timer() : m_running{false}, m_start_time{}, m_end_time{}
    {
    }

    void start()
    {
        if (!m_running)
        {
            m_start_time = std::chrono::high_resolution_clock::now();
            m_running = true;
        }
    }

    void stop()
    {
        if (m_running)
        {
            m_end_time = std::chrono::high_resolution_clock::now();
            m_elapsed_time = m_end_time - m_start_time;
            m_running = false;
        }
    }
    template <class T>
    T elapsed()
    {
        return std::chrono::duration_cast<T>(m_elapsed_time);
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_end_time;

    std::chrono::duration<long long, std::nano> m_elapsed_time;

    bool m_running;
};