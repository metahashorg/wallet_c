#ifndef TIMERCLASS_H
#define TIMERCLASS_H

#include <QObject>

#include <QThread>
#include <QTimer>

#include "duration.h"

// TODO переписать на него все, зависящее от таймера
class TimerClass : public QObject
{
    Q_OBJECT
public:
    TimerClass(const milliseconds &timerPeriod, QObject *parent);

    void exit();

    ~TimerClass();

    void start();

signals:

    void finished();

    void startedEvent();

    void timerEvent();

public slots:

protected:

    QThread thread1;

    QTimer qtimer;

private:

    bool isExited = false;

};

#endif // TIMERCLASS_H
