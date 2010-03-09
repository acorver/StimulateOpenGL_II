#include <QObject>
#include <QString>
#include <QApplication>
#include <QMessageBox>
#include <cstdlib>
#include <ctime>
#include <QMutex>
#include <QTextEdit>
#include <QTime>
#include <QThread>
#include <iostream>
#include "StimApp.h"
#include "Util.h"
#include "GLWindow.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wingdi.h>
#endif
#ifdef Q_WS_MACX
#  include <gl.h>
#else
#  include <GL/gl.h>
#endif

namespace Util {

void Connect(QObject *srco, const QString & src, QObject *desto, const QString & dest)
{
    if (!QObject::connect(srco, src.toUtf8(), desto, dest.toUtf8(), Qt::QueuedConnection)) {
        QString tmp;
        QMessageBox::critical(0, "Signal connection error", QString("Error connecting %1::%2 to %3::%4").arg( (tmp = srco->objectName()).isNull() ? "(unnamed)" : tmp ).arg(src.mid(1)).arg( (tmp = desto->objectName()).isNull() ? "(unnamed)" : tmp ).arg(dest.mid(1)));
        QApplication::exit(1);
        // only reached if event loop is not running
        std::exit(1);
    }
}

StimApp *stimApp()
{
     return StimApp::instance();
}

/// returns a QString signifying the last error error
QString glGetErrorString(int err)
{
    if (stimApp() && stimApp()->glWin()) stimApp()->glWin()->makeCurrent();
    else return "No GL Context!";
    switch (err) {
    case GL_INVALID_OPERATION:
        return "Invalid Operation";
    case GL_INVALID_ENUM:
        return "Invalid Enum";
    case GL_NO_ERROR:
        return "No Error";
    case GL_INVALID_VALUE:
        return "Invalid Value";
    case GL_OUT_OF_MEMORY:
        return "Out of Memory";
    case GL_STACK_OVERFLOW:
        return "Stack Overflow";
    case GL_STACK_UNDERFLOW:
        return "Stack Underflow";
    }
    return QString("UNKNOWN: ") + QString::number(err);
}

Log::Log()
    : doprt(true), str(""), s(&str, QIODevice::WriteOnly)
{
}

Log::~Log()
{    
    if (doprt) {        
        s.flush(); // does nothing probably..
        QString theString = QString("[Thread ") + QString::number((unsigned long)QThread::currentThreadId()) + " "  + QDateTime::currentDateTime().toString("M/dd/yy hh:mm:ss.zzz") + "] " + str;

        if (stimApp()) {
            stimApp()->logLine(theString, color);
        } else {
            // just print to console for now..
            std::cerr << theString.toUtf8().constData() << "\n";
        }
    }
}

Debug::~Debug()
{
    if (!stimApp() || !stimApp()->isDebugMode())
        doprt = false;
    color = Qt::darkBlue;
}


Error::~Error()
{
    color = Qt::darkRed;
}

Warning::~Warning()
{
    color = Qt::darkMagenta;
}


double random(double min, double max)
{
    static bool seeded = false;
    if (!seeded) { seeded = true; qsrand(std::time(0));  }
    int r = qrand();
    return double(r)/double(RAND_MAX-1) * (max-min) + min;
}

Status::Status(int to)
    : to(to), str(""), s(&str, QIODevice::WriteOnly)
{
    s.setRealNumberNotation(QTextStream::FixedNotation);
    s.setRealNumberPrecision(2);
}

Status::~Status()
{
    if (stimApp()) stimApp()->statusMsg(str, to);
    else {
        std::cerr << "STATUSMSG: " << str.toUtf8().constData() << "\n";
    }
}
	
const Vec2 Vec2Zero(0.,0.); // default 0 vec -- useful as a default argument to functions
const Vec2 Vec2Unit(1.,1.); // unit vector, that is, (1,1) -- usefule as a default argument to functions
const Vec2i Vec2iZero(0,0); // default 0 vec -- useful as a default argument to functions
const Vec2i Vec2iUnit(1,1); // unit vector, that is, (1,1) -- usefule as a default argument to functions

}
