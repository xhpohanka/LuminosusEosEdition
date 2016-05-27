// Copyright (c) 2016 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef POWERMATELISTENER_H
#define POWERMATELISTENER_H

#include <QFile>
#include <QThread>
#include <QDebug>

#if defined(Q_OS_LINUX)
    #include <linux/input.h>
#endif

class PowermateListener : public QThread {

    Q_OBJECT

public:

    bool isPressed = false;
    double lastPressTime;
    double lastRotationTime;
    QFile device_out;

    explicit PowermateListener(QObject *parent = 0);

#if defined(Q_OS_LINUX)
    void processEvent(const input_event& event);
#endif

protected:
    void run();

signals:
    void rotated(double val, bool isPressed);
    void pressed();
    void released(double duration);

public slots:
    void setBrightness(double);
    void startPulsing();

};

#endif // POWERMATELISTENER_H