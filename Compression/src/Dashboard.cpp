#include "Dashboard.hpp"
#include <iostream>
#include <iomanip>

void Dashboard::update(const DashboardState& s)
{
    std::lock_guard<std::mutex> lock(mtx);
    render(s);
}

void Dashboard::render(const DashboardState& s)
{
    // clear screen (ANSI escape)
    std::cout << "\033[2J\033[H";

    std::cout << "========================================\n";
    std::cout << "   IRIS COMPRESSION ENGINE (LIVE)\n";
    std::cout << "========================================\n\n";

    std::cout << "Event:        " << s.trigger << "\n";
    std::cout << "Frames:       " << s.startFrame
        << " → " << s.currentFrame
        << " / " << s.endFrame << "\n\n";

    std::cout << "Pressure:     " << std::fixed << std::setprecision(2)
        << s.pressure << "\n";

    std::cout << "Importance:   " << s.importance << "\n";

    std::cout << "CRF:          " << s.crf << "\n";
    std::cout << "FPS:          " << s.fps << "\n\n";

    std::cout << "CPU Load:     " << int(s.cpu * 100) << "%\n";
    std::cout << "Thermal:      " << int(s.thermal * 100) << "%\n";

    std::cout << "Drop Rate:    " << int(s.dropRate * 100) << "%\n";

    std::cout << "Encoder:      "
        << (s.encoderActive ? "ACTIVE" : "IDLE") << "\n";

    std::cout << "\n========================================\n";

    std::cout.flush();
}