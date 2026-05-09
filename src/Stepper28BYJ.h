#pragma once
#include <Arduino.h>

class Stepper28BYJ {
public:
    Stepper28BYJ(int p1, int p2, int p3, int p4)
        : pins{p1, p2, p3, p4}, _step(0) {
        for (int p : pins) pinMode(p, OUTPUT);
    }

    // direction: +1 = CW, -1 = CCW; delayMs >= 2, reliable from 5 ms
    void rotate(int steps, int dir, int delayMs = 5) {
        for (int i = 0; i < steps; i++) {
            _step = (_step + dir + 8) % 8;
            apply();
            delay(delayMs);
        }
    }

    void off() { for (int p : pins) digitalWrite(p, LOW); }

private:
    int pins[4];
    int _step;

    static constexpr bool seq[8][4] = {
        {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
        {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
    };

    void apply() {
        for (int i = 0; i < 4; i++)
            digitalWrite(pins[i], seq[_step][i]);
    }
};
