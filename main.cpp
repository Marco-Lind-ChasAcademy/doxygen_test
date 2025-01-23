/**
 * @file main.cpp
 * @author your name (you@domain.com)
 * @brief Traffic light simulation with pedestrian crossing button.
   The traffic light has three states: green, yellow, and red.
   The traffic light stays green until the pedestrian crossing button is pressed via keyboard or
   simulated button press. The pedestrian crossing button is pressed every 20-30 seconds to simulate
   crossing. When the button is pressed, the traffic light changes to red and the pedestrian can
   cross. The pedestrian crossing takes 20 seconds. The traffic light changes to green after the
   pedestrian has crossed. Button press can also be simulated by pressing any key. The program can
   be exited by pressing 'q'.
 * @version 0.1
 * @date 2025-01-23
 * 
 * @copyright Copyright (c) 2025
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#ifdef _WIN32
#    include <conio.h>
#else
#    include <termios.h>
#    include <unistd.h>
#endif

std::mutex mtx;
std::condition_variable cv;
std::atomic<bool> buttonPress(false);
std::atomic<bool> isRunning(true);
std::atomic<bool> pedestrianCrossing(false);

// Traffic light times in seconds
constexpr int GREEN_TIME  = 10;
constexpr int YELLOW_TIME = 3;
constexpr int RED_TIME    = 10;
constexpr int BUTTON_TIME = 20;

enum class State
{
    GREEN,
    YELLOW_AFTER_GREEN,
    RED,
    YELLOW_AFTER_RED
};

enum class PedestrianState
{
    GREEN,
    YELLOW_AFTER_GREEN,
    RED,
    YELLOW_AFTER_RED
};

void buttonSimulator(const std::atomic_bool* isRunning, std::atomic_bool* buttonPress,
                     std::mutex* mtx, std::condition_variable* cv,
                     std::atomic_bool* pedestrianCrossing);
void trafficLight();
void keyboardHandler();

int main()
{
    std::thread tButton(buttonSimulator, &isRunning, &buttonPress, &mtx, &cv, &pedestrianCrossing);
    std::thread tTrafficLight(trafficLight);
    std::thread tKeyboardHandler(keyboardHandler);

    tKeyboardHandler.join();
    isRunning = false;
    cv.notify_all();

    tButton.join();
    tTrafficLight.join();

    return 0;
}
/**
 * @brief Sleep function that checks for interrupt every 100ms for fast keyboard response
 * 
 * @param seconds 
 */
// 
// Second is multiplied by 10 to get the number of 100ms intervals for ish seconds.
void sleepWithInterrupt(int seconds)
{
    for (int i = 0; i < seconds * 10; ++i)
    {
        if (!isRunning)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (buttonPress)
            break;
    }
}

/**
 * @brief Keyboard handling for Windows and Mac/Linux, q for exit, any other key for button press
 * 
 */
void keyboardHandler()
{
#ifdef _WIN32
    while (isRunning)
    {
        if (_kbhit())
        {
            // Get the key press without echoing it to the console
            char ch = _getch();
            if (ch == 'q')
            {
                isRunning = false;
                cv.notify_all();
            }
            else if (!pedestrianCrossing)
            {
                buttonPress = true;
                cv.notify_all();
            }
        }
    }
    // Reduce CPU usage
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
#else
    // Save the current terminal settings and disable canonical mode and echo
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (isRunning)
    {
        char ch = getchar();
        if (ch == 'q')
        {
            isRunning = false;
            cv.notify_all();
        }
        else if (!pedestrianCrossing)
        {
            buttonPress = true;
            cv.notify_all();
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
}

// Display the current state of the traffic light
void displayLight(const State& state, const PedestrianState& pedState)
{
    std::string strState;
    std::string strPedState;

    switch (state)
    {
        case State::GREEN:
            strState = "\033[32mGREEN\033[0m";
            break;
        case State::YELLOW_AFTER_GREEN:
            strState = "\033[33mYELLOW\033[0m";
            break;
        case State::YELLOW_AFTER_RED:
            strState = "\033[33mYELLOW\033[0m";
            break;
        case State::RED:
            strState = "\033[31mRED\033[0m";
            break;
    }

    switch (pedState)
    {
        case PedestrianState::GREEN:
            strPedState = "\033[32mGREEN\033[0m";
            break;
        case PedestrianState::YELLOW_AFTER_GREEN:
            strPedState = "\033[33mYELLOW\033[0m";
            break;
        case PedestrianState::YELLOW_AFTER_RED:
            strPedState = "\033[33mYELLOW\033[0m";
            break;
        case PedestrianState::RED:
            strPedState = "\033[31mRED\033[0m";
            break;
    }

    std::lock_guard<std::mutex> lck(mtx);
    std::cout << "Traffic light state: " << strState << " Pedestrian light state: " << strPedState
              << (pedestrianCrossing ? " (Pedestrian crossing)" : "") << "\n";
}

void trafficLight()
{
    // Set initial state to green
    State state              = State::GREEN;
    PedestrianState pedState = PedestrianState::RED;

    while (isRunning)
    {
        // Wait for button press or state change
        bool buttonHandled = false;  // Avoid race condition and change state only once
        {
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait(lck, [] { return buttonPress || isRunning; });

            if (buttonPress && state != State::RED && !pedestrianCrossing)
            {
                buttonHandled      = true;
                buttonPress        = false;
                state              = State::YELLOW_AFTER_GREEN;
                pedState           = PedestrianState::YELLOW_AFTER_RED;
                pedestrianCrossing = true;
                std::cout << "Button is pressed.\n";
            }
        }

        // Don't change state if the program is exiting
        if (!isRunning)
            break;

        displayLight(state, pedState);

        // Change state based on current state
        switch (state)
        {
            case State::GREEN:
                sleepWithInterrupt(GREEN_TIME);
                if (pedestrianCrossing)
                {
                    state    = State::YELLOW_AFTER_GREEN;
                    pedState = PedestrianState::YELLOW_AFTER_GREEN;
                }
                break;
            case State::YELLOW_AFTER_GREEN:
                sleepWithInterrupt(YELLOW_TIME);
                state    = State::RED;
                pedState = PedestrianState::GREEN;
                break;
            case State::RED:
                sleepWithInterrupt(BUTTON_TIME);
                state    = State::YELLOW_AFTER_RED;
                pedState = PedestrianState::YELLOW_AFTER_GREEN;
                break;
            case State::YELLOW_AFTER_RED:
                sleepWithInterrupt(YELLOW_TIME);
                state              = State::GREEN;
                pedState           = PedestrianState::RED;
                pedestrianCrossing = false;
                break;
        }
    }
}

void buttonSimulator(const std::atomic_bool* isRunning, std::atomic_bool* buttonPress,
                     std::mutex* mtx, std::condition_variable* cv,
                     std::atomic_bool* pedestrianCrossing)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(20, 30);

    while (*isRunning)
    {
        int randNum = dist(gen);

        if (!isRunning->load())
            break;

        // std::this_thread::sleep_for(std::chrono::seconds(randNum));
        sleepWithInterrupt(randNum);

        // If the pedestrian is crossing, don't press the button
        if (!pedestrianCrossing->load())
        {
            std::lock_guard<std::mutex> lck(*mtx);
            *buttonPress = true;
            cv->notify_all();
        }
    }
}