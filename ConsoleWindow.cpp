#include <QTextEdit>
#include <QMenuBar>
#include <QApplication>
#include <QStatusBar>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include "ConsoleWindow.h"
#include "GLWindow.h"
#include "StimApp.h"
#include "Util.h"

ConsoleWindow::ConsoleWindow(QWidget *p, Qt::WindowFlags f)
    : QMainWindow(p, f)
{
    QTextEdit *te = new QTextEdit(this);
    te->setUndoRedoEnabled(false);
    te->setReadOnly(true);
    setCentralWidget(te);
    QMenuBar *mb = menuBar();
    QMenu *m = mb->addMenu("&File");
    m->addAction("Load Stim.  L", qApp, SLOT(loadStim()));
    m->addAction("Unload Stim.  ESC", qApp, SLOT(unloadStim()));
    m->addAction("Pause/Unpause Stim.  SPACE", qApp, SLOT(pauseUnpause()));
    m->addAction("Check .FMV file for errors...", qApp, SLOT(checkFMV()));
    m->addSeparator();
    m->addAction("&Quit", qApp, SLOT(quit()));    
    statusBar()->showMessage("");

    m = mb->addMenu("&Options");
    QAction *action = m->addAction("&Debug Mode D", stimApp(), SLOT(setDebugMode(bool)));
    action->setCheckable(true);
    action->setChecked(stimApp()->isDebugMode());
    action = m->addAction("Excessive Debug", stimApp(), SLOT(setExcessiveDebugMode(bool)));
    action->setCheckable(true);
    action->setChecked(Util::excessiveDebug);
	action = m->addAction("&No Dropped Frame Warnings", stimApp(), SLOT(setNoDropFrameWarn(bool)));
    action->setCheckable(true);
    action->setChecked(stimApp()->isNoDropFrameWarn());
	action = m->addAction("&Save Frame Vars", stimApp(), SLOT(setSaveFrameVars(bool)));
	action->setCheckable(true);
	action->setChecked(stimApp()->isSaveFrameVars());
	action = m->addAction("Save Param &History", stimApp(), SLOT(setSaveParamHistory(bool)));
	action->setCheckable(true);
	action->setChecked(stimApp()->isSaveParamHistory());
	action = m->addAction("Dump Frames To Disk (SLOW!)", stimApp(), SLOT(setFrameDumpMode(bool)));
	action->setCheckable(true);
	action->setChecked(stimApp()->isFrameDumpMode());
	vsyncDisabledAction = action = m->addAction("Disable VSync", stimApp(), SLOT(setVSyncDisabled(bool)));
	action->setCheckable(true);
	action->setChecked(stimApp()->isVSyncDisabled());
	m->addSeparator();
    action = m->addAction("Choose &Output Directory...", stimApp(), SLOT(pickOutputDir()));
#ifndef Q_OS_WIN
	action = m->addAction("Calibrate &Refresh R", stimApp(), SLOT(calibrateRefresh()));
#endif
    action = m->addAction("Hide &Console C", stimApp(), SLOT(hideUnhideConsole()));
    action = m->addAction("&Align/Unalign GL Window A", stimApp(), SLOT(alignGLWindow()));
    action = m->addAction("&SpikeGL Integration...", stimApp(), SLOT(spikeGLIntegrationDialog()));
	action = m->addAction("&Global Parameter Defaults...", stimApp(), SLOT(globalDefaultsDialog()));
    m = mb ->addMenu("&Help");
    action = m->addAction("&About", stimApp(), SLOT(about()));
    action = m->addAction("About &Qt", stimApp(), SLOT(aboutQt()));
}

QTextEdit *ConsoleWindow::textEdit() const
{
    return centralWidget() ? dynamic_cast<QTextEdit *>(centralWidget()) : 0;
}

void ConsoleWindow::closeEvent(QCloseEvent *e)
{
    e->accept();
    qApp->quit();
}
