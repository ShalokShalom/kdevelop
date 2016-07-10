/*
 * Unit tests for LLDB debugger plugin
   Copyright 2009 Niko Sams <niko.sams@gmail.com>
   Copyright 2013 Vlas Puhov <vlas.puhov@mail.ru>
 * Copyright 2016  Aetf <aetf@unlimitedcodeworks.xyz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "test_lldb.h"

#include "controllers/framestackmodel.h"
#include "debugsession.h"
#include "testhelper.h"

#include <execute/iexecuteplugin.h>
#include <debugger/breakpoint/breakpointmodel.h>
#include <debugger/variable/variablecollection.h>
#include <interfaces/idebugcontroller.h>
#include <interfaces/ilaunchconfiguration.h>
#include <interfaces/iplugincontroller.h>
#include <tests/autotestshell.h>
#include <tests/testcore.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFileInfo>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#define WAIT_FOR_STATE(session, state) \
    do { if (!KDevMI::LLDB::waitForState((session), (state), __FILE__, __LINE__)) return; } while (0)

#define WAIT_FOR_STATE_AND_IDLE(session, state) \
    do { if (!KDevMI::LLDB::waitForState((session), (state), __FILE__, __LINE__, true)) return; } while (0)

#define WAIT_FOR_A_WHILE(session, ms) \
    do { if (!KDevMI::LLDB::waitForAWhile((session), (ms), __FILE__, __LINE__)) return; } while (0)

#define WAIT_FOR(session, condition) \
    do { \
        KDevMI::LLDB::TestWaiter w((session), #condition, __FILE__, __LINE__); \
        while (w.waitUnless((condition))) /* nothing */ ; \
    } while(0)

#define COMPARE_DATA(index, expected) \
    do { if (!KDevMI::LLDB::compareData((index), (expected), __FILE__, __LINE__)) return; } while (0)

#define SKIP_IF_ATTACH_FORBIDDEN() \
    do { \
        if (isAttachForbidden(__FILE__, __LINE__)) \
            return; \
    } while(0)

#define findSourceFile(name) \
    findSourceFile(__FILE__, (name))

using namespace KDevelop;
using namespace KDevMI::LLDB;

namespace {
class TestLaunchConfiguration : public ILaunchConfiguration
{
public:
    TestLaunchConfiguration(const QUrl& executable = findExecutable("lldb_debugee"),
                            const QUrl& workingDirectory = QUrl()) {
        qDebug() << "FIND" << executable;
        c = new KConfig();
        c->deleteGroup("launch");
        cfg = c->group("launch");
        cfg.writeEntry("isExecutable", true);
        cfg.writeEntry("Executable", executable);
        cfg.writeEntry("Working Directory", workingDirectory);
    }
    ~TestLaunchConfiguration() override {
        delete c;
    }
    const KConfigGroup config() const override { return cfg; }
    KConfigGroup config() override { return cfg; };
    QString name() const override { return QString("Test-Launch"); }
    KDevelop::IProject* project() const override { return 0; }
    KDevelop::LaunchConfigurationType* type() const override { return 0; }
private:
    KConfigGroup cfg;
    KConfig *c;
};

class TestFrameStackModel : public LldbFrameStackModel
{
public:

    TestFrameStackModel(DebugSession* session)
        : LldbFrameStackModel(session), fetchFramesCalled(0), fetchThreadsCalled(0) {}

    void fetchFrames(int threadNumber, int from, int to) override
    {
        fetchFramesCalled++;
        LldbFrameStackModel::fetchFrames(threadNumber, from, to);
    }

    void fetchThreads() override
    {
        fetchThreadsCalled++;
        LldbFrameStackModel::fetchThreads();
    }

    int fetchFramesCalled;
    int fetchThreadsCalled;
};

class TestDebugSession : public DebugSession
{
    Q_OBJECT
public:
    TestDebugSession() : DebugSession()
    {
        setSourceInitFile(false);
        m_frameStackModel = new TestFrameStackModel(this);
        KDevelop::ICore::self()->debugController()->addSession(this);
    }

    TestFrameStackModel* frameStackModel() const override
    {
        return m_frameStackModel;
    }

private:
    TestFrameStackModel* m_frameStackModel;
};

} // end of anonymous namespace


BreakpointModel* LldbTest::breakpoints()
{
    return m_core->debugController()->breakpointModel();
}

VariableCollection *LldbTest::variableCollection()
{
    return m_core->debugController()->variableCollection();
}

Variable *LldbTest::watchVariableAt(int i)
{
    auto watchRoot = variableCollection()->indexForItem(variableCollection()->watches(), 0);
    auto idx = variableCollection()->index(i, 0, watchRoot);
    return dynamic_cast<Variable*>(variableCollection()->itemForIndex(idx));
}

Variable *LldbTest::localVariableAt(int i)
{
    auto localRoot = variableCollection()->indexForItem(variableCollection()->locals(), 0);
    auto idx = variableCollection()->index(i, 0, localRoot);
    return dynamic_cast<Variable*>(variableCollection()->itemForIndex(idx));
}

// Called before the first testfunction is executed
void LldbTest::initTestCase()
{
    AutoTestShell::init();
    m_core = TestCore::initialize(Core::NoUi);

    m_iface = m_core->pluginController()
                ->pluginForExtension("org.kdevelop.IExecutePlugin", "kdevexecute")
                ->extension<IExecutePlugin>();
    Q_ASSERT(m_iface);

    m_debugeeFileName = findSourceFile("debugee.cpp");
}

// Called after the last testfunction was executed
void LldbTest::cleanupTestCase()
{
    TestCore::shutdown();
}

// Called before each testfunction is executed
void LldbTest::init()
{
    //remove all breakpoints - so we can set our own in the test
    KConfigGroup bpCfg = KSharedConfig::openConfig()->group("breakpoints");
    bpCfg.writeEntry("number", 0);
    bpCfg.sync();

    breakpoints()->removeRows(0, breakpoints()->rowCount());

    while (variableCollection()->watches()->childCount() > 0) {
        auto var = watchVariableAt(0);
        if (!var) break;
        var->die();
    }
}

void LldbTest::cleanup()
{
    // Called after every testfunction
}

void LldbTest::testStdout()
{
    TestDebugSession *session = new TestDebugSession;

    QSignalSpy outputSpy(session, &TestDebugSession::inferiorStdoutLines);

    TestLaunchConfiguration cfg;
    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE(session, KDevelop::IDebugSession::EndedState);

    QCOMPARE(outputSpy.count(), 2);
    QList<QVariant> arguments = outputSpy.takeFirst();
    QCOMPARE(arguments.count(), 1);
    QCOMPARE(arguments.first().toStringList(), QStringList() << "Hello, world!");

    arguments = outputSpy.takeFirst();
    QCOMPARE(arguments.count(), 1);
    QCOMPARE(arguments.first().toStringList(), QStringList() << "Hello");
}

void LldbTest::testBreakpoint()
{
    TestDebugSession *session = new TestDebugSession;

    TestLaunchConfiguration cfg;

    KDevelop::Breakpoint * b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 29);
    QCOMPARE(b->state(), KDevelop::Breakpoint::NotStartedState);

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    QCOMPARE(b->state(), KDevelop::Breakpoint::CleanState);
    QCOMPARE(session->currentLine(), 29);

    session->stepInto();
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    session->stepInto();
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
    QCOMPARE(b->state(), KDevelop::Breakpoint::NotStartedState);
}

void LldbTest::testBreakOnStart()
{
    QSKIP("SKipping... Pending breakpoints not work with the current version of lldb-mi");

    TestDebugSession *session = new TestDebugSession;

    TestLaunchConfiguration cfg;
    cfg.config().writeEntry(KDevMI::breakOnStartEntry, true);

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 28); // line 28 is the start of main function in debugee.cpp

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testDisableBreakpoint()
{
    QSKIP("Skipping... In lldb-mi -d flag has no effect when mixed with -f");
    //Description: We must stop only on the third breakpoint

    int firstBreakLine=28;
    int secondBreakLine=23;
    int thirdBreakLine=24;
    int fourthBreakLine=31;

    TestDebugSession *session = new TestDebugSession;

    TestLaunchConfiguration cfg;

    KDevelop::Breakpoint *b;

    b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), firstBreakLine);
    b->setData(KDevelop::Breakpoint::EnableColumn, Qt::Unchecked);


    //this is needed to emulate debug from GUI. If we are in edit mode, the debugSession doesn't exist.
    m_core->debugController()->breakpointModel()->blockSignals(true);
    b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), secondBreakLine);
    b->setData(KDevelop::Breakpoint::EnableColumn, Qt::Unchecked);
    //all disabled breakpoints were added

    auto *thirdBreak = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName),
                                                        thirdBreakLine);
    m_core->debugController()->breakpointModel()->blockSignals(false);


    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), thirdBreak->line());

    //disable existing breakpoint
    thirdBreak->setData(KDevelop::Breakpoint::EnableColumn, Qt::Unchecked);

    //add another disabled breakpoint
    b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), fourthBreakLine);
    WAIT_FOR_A_WHILE(session, 300);
    b->setData(KDevelop::Breakpoint::EnableColumn, Qt::Unchecked);

    WAIT_FOR_A_WHILE(session, 300);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testChangeLocationBreakpoint()
{
    TestDebugSession *session = new TestDebugSession;

    TestLaunchConfiguration cfg;

    auto *b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 27);

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 27);

    WAIT_FOR_A_WHILE(session, 100);
    b->setLine(28);
    WAIT_FOR_A_WHILE(session, 100);
    session->run();

    WAIT_FOR_A_WHILE(session, 100);
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 28);
    WAIT_FOR_A_WHILE(session, 500);
    breakpoints()->setData(breakpoints()->index(0, KDevelop::Breakpoint::LocationColumn), QString(m_debugeeFileName+":30"));
    QCOMPARE(b->line(), 29);
    WAIT_FOR_A_WHILE(session, 100);
    QCOMPARE(b->line(), 29);
    session->run();
    WAIT_FOR_A_WHILE(session, 100);
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 29);
    session->run();

    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testDeleteBreakpoint()
{
    TestDebugSession *session = new TestDebugSession;

    TestLaunchConfiguration cfg;

    QCOMPARE(breakpoints()->rowCount(), 0);
    //add breakpoint before startDebugging
    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 21);
    QCOMPARE(breakpoints()->rowCount(), 1);
    breakpoints()->removeRow(0);
    QCOMPARE(breakpoints()->rowCount(), 0);

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 22);

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    breakpoints()->removeRow(0);
    session->run();

    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testPendingBreakpoint()
{
    QSKIP("Skipping... Pending breakpoint not work on lldb-mi");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 28);

    auto * b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(findSourceFile("test_lldb.cpp")), 10);
    QCOMPARE(b->state(), Breakpoint::NotStartedState);

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    QCOMPARE(b->state(), Breakpoint::PendingState);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testUpdateBreakpoint()
{
    // Description: user might insert breakpoints using lldb console. model should
    // pick up the manually set breakpoint
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    // break at line 29
    auto b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 28);
    QCOMPARE(breakpoints()->rowCount(), 1);

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState); // stop at line 29

    session->stepInto();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState); // stop after step

    QCOMPARE(session->currentLine(), 23-1); // at the begining of foo():23: ++i;

    session->addUserCommand(QString("break set --file %1 --line %2").arg(m_debugeeFileName).arg(33));
    WAIT_FOR_A_WHILE(session, 20);

    QCOMPARE(breakpoints()->rowCount(), 2);
    b = breakpoints()->breakpoint(1);
    QCOMPARE(b->url(), QUrl::fromLocalFile(m_debugeeFileName));
    QCOMPARE(b->line(), 33-1);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState); // stop at line 25

    QCOMPARE(session->currentLine(), 33-1);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testIgnoreHitsBreakpoint()
{
    QSKIP("Skipping... lldb-mi doesn't provide breakpoint hit count update");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    KDevelop::Breakpoint * b1 = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 21);
    b1->setIgnoreHits(1);

    KDevelop::Breakpoint * b2 = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 22);

    session->startDebugging(&cfg, m_iface);

    //WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR(session, session->state() == DebugSession::PausedState && b2->hitCount() == 1);
    b2->setIgnoreHits(1);
    session->run();
    //WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR(session, session->state() == DebugSession::PausedState && b1->hitCount() == 1);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testConditionBreakpoint()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    auto b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 39);
    b->setCondition("x[0] == 'H'");

    b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 23);
    b->setCondition("i==2");

    b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR(session, session->state() == DebugSession::PausedState && session->currentLine() == 24);
    b->setCondition("i == 0");
    WAIT_FOR_A_WHILE(session, 100);
    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    QCOMPARE(session->currentLine(), 23);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    QCOMPARE(session->currentLine(), 39);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testBreakOnWriteBreakpoint()
{
    QSKIP("Skipping... lldb-mi doesn't have proper watchpoint support");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);

    breakpoints()->addWatchpoint("i");
    WAIT_FOR_A_WHILE(session, 100);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 23);
    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testBreakOnWriteWithConditionBreakpoint()
{
    QSKIP("Skipping... lldb-mi doesn't have proper watchpoint support");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);

    KDevelop::Breakpoint *b = breakpoints()->addWatchpoint("i");
    b->setCondition("i==2");
    QTest::qWait(100);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 23);
    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testBreakOnReadBreakpoint()
{
    QSKIP("Skipping... lldb-mi doesn't have proper watchpoint support");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addReadWatchpoint("foo::i");

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 23);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testBreakOnReadBreakpoint2()
{
    QSKIP("Skipping... lldb-mi doesn't have proper watchpoint support");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);

    breakpoints()->addReadWatchpoint("i");

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 22);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testBreakOnAccessBreakpoint()
{
    QSKIP("Skipping... lldb-mi doesn't have proper watchpoint support");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);

    breakpoints()->addAccessWatchpoint("i");

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 22);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 23);


    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 24);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testInsertBreakpointWhileRunning()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg(findExecutable("lldb_debugeeslow"));
    QString fileName = findSourceFile("debugeeslow.cpp");

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE(session, DebugSession::ActiveState);
    WAIT_FOR_A_WHILE(session, 2000);

    qDebug() << "adding breakpoint";
    KDevelop::Breakpoint *b = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(fileName), 25);
    WAIT_FOR_A_WHILE(session, 500);

    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 500);

    QCOMPARE(session->currentLine(), 25);
    b->setDeleted();
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testInsertBreakpointWhileRunningMultiple()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg(findExecutable("lldb_debugeeslow"));
    QString fileName = findSourceFile("debugeeslow.cpp");

    session->startDebugging(&cfg, m_iface);

    WAIT_FOR_STATE(session, DebugSession::ActiveState);
    WAIT_FOR_A_WHILE(session, 2000);

    qDebug() << "adding breakpoint";
    auto b1 = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(fileName), 24);
    auto b2 = breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(fileName), 25);

    WAIT_FOR_A_WHILE(session, 500);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    WAIT_FOR_A_WHILE(session, 500);
    QCOMPARE(session->currentLine(), 24);

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    WAIT_FOR_A_WHILE(session, 500);
    QCOMPARE(session->currentLine(), 25);
    b1->setDeleted();
    b2->setDeleted();

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testInsertBreakpointFunctionName()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint("main");

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 27);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testManualBreakpoint()
{
    QSKIP("Skipping... lldb-mi output malformated response which breaks this");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint("main");

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->currentLine(), 27);

    breakpoints()->removeRows(0, 1);
    WAIT_FOR_A_WHILE(session, 100);
    QCOMPARE(breakpoints()->rowCount(), 0);

    session->addCommand(MI::NonMI, "break set --file debugee.cpp --line 23");
    WAIT_FOR_A_WHILE(session, 100);
    QCOMPARE(breakpoints()->rowCount(), 1);

    auto b = breakpoints()->breakpoint(0);
    QCOMPARE(b->line(), 22);

    session->addCommand(MI::NonMI, "break disable 2");
    session->addCommand(MI::NonMI, "break modify -c 'i == 1' 2");
    session->addCommand(MI::NonMI, "break modify -i 1 2");
    WAIT_FOR_A_WHILE(session, 1000);
    QCOMPARE(b->enabled(), false);
    QCOMPARE(b->condition(), QString("i == 1"));
    QCOMPARE(b->ignoreHits(), 1);

    session->addCommand(MI::NonMI, "break delete 2");
    WAIT_FOR_A_WHILE(session, 100);
    QCOMPARE(breakpoints()->rowCount(), 0);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testShowStepInSource()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    QSignalSpy showStepInSourceSpy(session, &TestDebugSession::showStepInSource);

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 29);

    session->startDebugging(&cfg, m_iface);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    session->stepInto();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    session->stepInto();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);

    {
        QCOMPARE(showStepInSourceSpy.count(), 3);
        QList<QVariant> arguments = showStepInSourceSpy.takeFirst();
        QCOMPARE(arguments.first().value<QUrl>(), QUrl::fromLocalFile(m_debugeeFileName));
        QCOMPARE(arguments.at(1).toInt(), 29);

        arguments = showStepInSourceSpy.takeFirst();
        QCOMPARE(arguments.first().value<QUrl>(), QUrl::fromLocalFile(m_debugeeFileName));
        QCOMPARE(arguments.at(1).toInt(), 22);

        arguments = showStepInSourceSpy.takeFirst();
        QCOMPARE(arguments.first().value<QUrl>(), QUrl::fromLocalFile(m_debugeeFileName));
        QCOMPARE(arguments.at(1).toInt(), 23);
    }
}

void LldbTest::testStack()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    TestFrameStackModel *stackModel = session->frameStackModel();

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 21);

    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    QModelIndex tIdx = stackModel->index(0,0);
    QCOMPARE(stackModel->rowCount(QModelIndex()), 1);
    QCOMPARE(stackModel->columnCount(QModelIndex()), 3);
    COMPARE_DATA(tIdx, "#1 at foo()");

    QCOMPARE(stackModel->rowCount(tIdx), 4);
    QCOMPARE(stackModel->columnCount(tIdx), 3);
    COMPARE_DATA(tIdx.child(0, 0), "0");
    COMPARE_DATA(tIdx.child(0, 1), "foo()");
    COMPARE_DATA(tIdx.child(0, 2), m_debugeeFileName+":23");
    COMPARE_DATA(tIdx.child(1, 0), "1");
    COMPARE_DATA(tIdx.child(1, 1), "main");
    COMPARE_DATA(tIdx.child(1, 2), m_debugeeFileName+":29");
    COMPARE_DATA(tIdx.child(2, 0), "2");
    COMPARE_DATA(tIdx.child(2, 1), "__libc_start_main");
    COMPARE_DATA(tIdx.child(3, 0), "3");
    COMPARE_DATA(tIdx.child(3, 1), "_start");


    session->stepOut();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    COMPARE_DATA(tIdx, "#1 at main");
    QCOMPARE(stackModel->rowCount(tIdx), 3);
    COMPARE_DATA(tIdx.child(0, 0), "0");
    COMPARE_DATA(tIdx.child(0, 1), "main");
    COMPARE_DATA(tIdx.child(0, 2), m_debugeeFileName+":30");
    COMPARE_DATA(tIdx.child(1, 0), "1");
    COMPARE_DATA(tIdx.child(1, 1), "__libc_start_main");
    COMPARE_DATA(tIdx.child(2, 0), "2");
    COMPARE_DATA(tIdx.child(2, 1), "_start");

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testStackFetchMore()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg(findExecutable("lldb_debugeerecursion"));
    QString fileName = findSourceFile("debugeerecursion.cpp");

    TestFrameStackModel *stackModel = session->frameStackModel();

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(fileName), 25);

    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    QCOMPARE(session->frameStackModel()->fetchFramesCalled, 1);

    QModelIndex tIdx = stackModel->index(0,0);
    QCOMPARE(stackModel->rowCount(QModelIndex()), 1);
    QCOMPARE(stackModel->columnCount(QModelIndex()), 3);
    COMPARE_DATA(tIdx, "#1 at foo()");

    QCOMPARE(stackModel->rowCount(tIdx), 21);
    COMPARE_DATA(tIdx.child(0, 0), "0");
    COMPARE_DATA(tIdx.child(0, 1), "foo()");
    COMPARE_DATA(tIdx.child(0, 2), fileName+":26");
    COMPARE_DATA(tIdx.child(1, 0), "1");
    COMPARE_DATA(tIdx.child(1, 1), "foo()");
    COMPARE_DATA(tIdx.child(1, 2), fileName+":24");
    COMPARE_DATA(tIdx.child(2, 0), "2");
    COMPARE_DATA(tIdx.child(2, 1), "foo()");
    COMPARE_DATA(tIdx.child(2, 2), fileName+":24");
    COMPARE_DATA(tIdx.child(19, 0), "19");
    COMPARE_DATA(tIdx.child(20, 0), "20");

    stackModel->fetchMoreFrames();
    WAIT_FOR_A_WHILE(session, 200);
    QCOMPARE(stackModel->fetchFramesCalled, 2);
    QCOMPARE(stackModel->rowCount(tIdx), 41);
    COMPARE_DATA(tIdx.child(20, 0), "20");
    COMPARE_DATA(tIdx.child(21, 0), "21");
    COMPARE_DATA(tIdx.child(22, 0), "22");
    COMPARE_DATA(tIdx.child(39, 0), "39");
    COMPARE_DATA(tIdx.child(40, 0), "40");

    stackModel->fetchMoreFrames();
    WAIT_FOR_A_WHILE(session, 200);
    QCOMPARE(stackModel->fetchFramesCalled, 3);
    QCOMPARE(stackModel->rowCount(tIdx), 121);
    COMPARE_DATA(tIdx.child(40, 0), "40");
    COMPARE_DATA(tIdx.child(41, 0), "41");
    COMPARE_DATA(tIdx.child(42, 0), "42");
    COMPARE_DATA(tIdx.child(119, 0), "119");
    COMPARE_DATA(tIdx.child(120, 0), "120");

    stackModel->fetchMoreFrames();
    WAIT_FOR_A_WHILE(session, 200);
    QCOMPARE(stackModel->fetchFramesCalled, 4);
    QCOMPARE(stackModel->rowCount(tIdx), 301);
    COMPARE_DATA(tIdx.child(120, 0), "120");
    COMPARE_DATA(tIdx.child(121, 0), "121");
    COMPARE_DATA(tIdx.child(122, 0), "122");
    COMPARE_DATA(tIdx.child(298, 0), "298");
    COMPARE_DATA(tIdx.child(298, 1), "main");
    COMPARE_DATA(tIdx.child(298, 2), fileName+":30");
    COMPARE_DATA(tIdx.child(299, 0), "299");
    COMPARE_DATA(tIdx.child(299, 1), "__libc_start_main");
    COMPARE_DATA(tIdx.child(300, 0), "300");
    COMPARE_DATA(tIdx.child(300, 1), "_start");

    stackModel->fetchMoreFrames(); //nothing to fetch, we are at the end
    WAIT_FOR_A_WHILE(session, 200);
    QCOMPARE(stackModel->fetchFramesCalled, 4);
    QCOMPARE(stackModel->rowCount(tIdx), 301);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testStackDeactivateAndActive()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    TestFrameStackModel *stackModel = session->frameStackModel();

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 21);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    QModelIndex tIdx = stackModel->index(0,0);

    session->stepOut();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);
    COMPARE_DATA(tIdx, "#1 at main");
    QCOMPARE(stackModel->rowCount(tIdx), 3);
    COMPARE_DATA(tIdx.child(0, 0), "0");
    COMPARE_DATA(tIdx.child(0, 1), "main");
    COMPARE_DATA(tIdx.child(0, 2), m_debugeeFileName+":30");
    COMPARE_DATA(tIdx.child(1, 0), "1");
    COMPARE_DATA(tIdx.child(1, 1), "__libc_start_main");
    COMPARE_DATA(tIdx.child(2, 0), "2");
    COMPARE_DATA(tIdx.child(2, 1), "_start");

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testStackSwitchThread()
{
    QSKIP("Skipping... lldb-mi crashes when break at a location with multiple threads running");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg(findExecutable("lldb_debugeethreads"));
    QString fileName = findSourceFile("debugeethreads.cpp");

    TestFrameStackModel *stackModel = session->frameStackModel();

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(fileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    QCOMPARE(stackModel->rowCount(), 4);

    QModelIndex tIdx = stackModel->index(0,0);
    COMPARE_DATA(tIdx, "#1 at main");
    QCOMPARE(stackModel->rowCount(tIdx), 1);
    COMPARE_DATA(tIdx.child(0, 0), "0");
    COMPARE_DATA(tIdx.child(0, 1), "main");
    COMPARE_DATA(tIdx.child(0, 2), fileName+":39");

    tIdx = stackModel->index(1,0);
    QVERIFY(stackModel->data(tIdx).toString().startsWith("#2 at "));
    stackModel->setCurrentThread(2);
    WAIT_FOR_A_WHILE(session, 200);
    int rows = stackModel->rowCount(tIdx);
    QVERIFY(rows > 3);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testAttach()
{
    SKIP_IF_ATTACH_FORBIDDEN();

    QString fileName = findSourceFile("debugeeslow.cpp");

    KProcess debugeeProcess;
    debugeeProcess << "nice" << findExecutable("lldb_debugeeslow").toLocalFile();
    debugeeProcess.start();
    QVERIFY(debugeeProcess.waitForStarted());
    QTest::qWait(100);

    TestDebugSession *session = new TestDebugSession;
    session->attachToProcess(debugeeProcess.pid());

    WAIT_FOR_A_WHILE(session, 100);

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(fileName), 35);

    // lldb-mi sliently stops when attaching to a process. Force it continue to run.
    session->addCommand(MI::ExecContinue, QString(), MI::CmdMaybeStartsRunning);
    WAIT_FOR_A_WHILE(session, 2000);
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    QCOMPARE(session->currentLine(), 35);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testManualAttach()
{
    QSKIP("Skipping... No remote debug support implemented");
    SKIP_IF_ATTACH_FORBIDDEN();

    QString fileName = findSourceFile("debugeeslow.cpp");

    KProcess debugeeProcess;
    debugeeProcess << "nice" << findExecutable("lldb_debugeeslow").toLocalFile();
    debugeeProcess.start();
    QVERIFY(debugeeProcess.waitForStarted());

    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    // Start remote debugging
    //QVERIFY(session->startDebugging(&cfg, m_iface));

    session->addCommand(MI::NonMI, QString("attach %0").arg(debugeeProcess.pid()));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    session->run();
    WAIT_FOR_A_WHILE(session, 2000); // give the slow inferior some extra time to run
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testCoreFile()
{
    // TODO: support for coredumpctl

    QFile f("core");
    if (f.exists()) f.remove();

    KProcess debugeeProcess;
    debugeeProcess.setOutputChannelMode(KProcess::MergedChannels);
    debugeeProcess << "bash" << "-c" << "ulimit -c unlimited; " + findExecutable("lldb_debugeecrash").toLocalFile();
    debugeeProcess.start();
    debugeeProcess.waitForFinished();
    qDebug() << debugeeProcess.readAll();
    if (!QFile::exists("core")) {
        QSKIP("no core dump found, check your system configuration (see /proc/sys/kernel/core_pattern).", SkipSingle);
    }

    TestDebugSession *session = new TestDebugSession;
    session->examineCoreFile(findExecutable("lldb_debugeecrash"), QUrl::fromLocalFile(QDir::currentPath()+"/core"));

    TestFrameStackModel *stackModel = session->frameStackModel();

    WAIT_FOR_STATE(session, DebugSession::StoppedState);

    QModelIndex tIdx = stackModel->index(0,0);
    QCOMPARE(stackModel->rowCount(QModelIndex()), 1);
    QCOMPARE(stackModel->columnCount(QModelIndex()), 3);
    COMPARE_DATA(tIdx, "#1 at foo");

    session->stopDebugger();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesLocals()
{
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    session->variableController()->setAutoUpdate(IVariableController::UpdateLocals);

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);

    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    QCOMPARE(variableCollection()->rowCount(), 2);
    QModelIndex i = variableCollection()->index(1, 0);
    COMPARE_DATA(i, "Locals");

    QCOMPARE(variableCollection()->rowCount(i), 1);
    COMPARE_DATA(variableCollection()->index(0, 0, i), "j");
    COMPARE_DATA(variableCollection()->index(0, 1, i), "1");

    session->run();
    WAIT_FOR_STATE_AND_IDLE(session, DebugSession::PausedState);

    COMPARE_DATA(variableCollection()->index(0, 0, i), "j");
    COMPARE_DATA(variableCollection()->index(0, 1, i), "2");

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesLocalsStruct()
{
    TestDebugSession *session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateLocals);

    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 1000);

    QModelIndex i = variableCollection()->index(1, 0);
    QCOMPARE(variableCollection()->rowCount(i), 4);

    int structIndex = 0;
    for(int j=0; j<3; ++j) {
        if (variableCollection()->index(j, 0, i).data().toString() == "ts") {
            structIndex = j;
        }
    }

    COMPARE_DATA(variableCollection()->index(structIndex, 0, i), "ts");
    COMPARE_DATA(variableCollection()->index(structIndex, 1, i), "{...}");
    QModelIndex ts = variableCollection()->index(structIndex, 0, i);
    COMPARE_DATA(variableCollection()->index(0, 0, ts), "...");
    variableCollection()->expanded(ts);
    WAIT_FOR_A_WHILE(session, 100);
    COMPARE_DATA(variableCollection()->index(0, 0, ts), "a");
    COMPARE_DATA(variableCollection()->index(0, 1, ts), "0");
    COMPARE_DATA(variableCollection()->index(1, 0, ts), "b");
    COMPARE_DATA(variableCollection()->index(1, 1, ts), "1");
    COMPARE_DATA(variableCollection()->index(2, 0, ts), "c");
    COMPARE_DATA(variableCollection()->index(2, 1, ts), "2");

    session->stepInto();
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 1000);
    COMPARE_DATA(variableCollection()->index(structIndex, 0, i), "ts");
    COMPARE_DATA(variableCollection()->index(structIndex, 1, i), "{...}");
    COMPARE_DATA(variableCollection()->index(0, 1, ts), "1");

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesWatches()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    m_core->debugController()->variableCollection()->variableWidgetShown();

    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    variableCollection()->watches()->add("ts");
    WAIT_FOR_A_WHILE(session, 300);

    QModelIndex i = variableCollection()->index(0, 0);
    QCOMPARE(variableCollection()->rowCount(i), 1);
    COMPARE_DATA(variableCollection()->index(0, 0, i), "ts");
    COMPARE_DATA(variableCollection()->index(0, 1, i), "{...}");
    QModelIndex ts = variableCollection()->index(0, 0, i);
    COMPARE_DATA(variableCollection()->index(0, 0, ts), "...");
    variableCollection()->expanded(ts);
    WAIT_FOR_A_WHILE(session, 100);
    COMPARE_DATA(variableCollection()->index(0, 0, ts), "a");
    COMPARE_DATA(variableCollection()->index(0, 1, ts), "0");
    COMPARE_DATA(variableCollection()->index(1, 0, ts), "b");
    COMPARE_DATA(variableCollection()->index(1, 1, ts), "1");
    COMPARE_DATA(variableCollection()->index(2, 0, ts), "c");
    COMPARE_DATA(variableCollection()->index(2, 1, ts), "2");

    session->stepInto();
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 100);
    COMPARE_DATA(variableCollection()->index(0, 0, i), "ts");
    COMPARE_DATA(variableCollection()->index(0, 1, i), "{...}");
    COMPARE_DATA(variableCollection()->index(0, 1, ts), "1");

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesWatchesQuotes()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateWatches);

    TestLaunchConfiguration cfg;

    const QString testString("test");
    const QString quotedTestString("\"" + testString + "\"");

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    variableCollection()->watches()->add(quotedTestString); //just a constant string
    WAIT_FOR_A_WHILE(session, 300);

    QModelIndex i = variableCollection()->index(0, 0);
    QCOMPARE(variableCollection()->rowCount(i), 1);
    COMPARE_DATA(variableCollection()->index(0, 0, i), quotedTestString);
    COMPARE_DATA(variableCollection()->index(0, 1, i), "[" + QString::number(testString.length() + 1) + "]");

    QModelIndex testStr = variableCollection()->index(0, 0, i);
    COMPARE_DATA(variableCollection()->index(0, 0, testStr), "...");
    variableCollection()->expanded(testStr);
    WAIT_FOR_A_WHILE(session, 100);
    int len = testString.length();
    for (int ind = 0; ind < len; ind++)
    {
        COMPARE_DATA(variableCollection()->index(ind, 0, testStr), QString::number(ind));
        QChar c = testString.at(ind);
        QString value = QString::number(c.toLatin1()) + " '" + c + "'";
        COMPARE_DATA(variableCollection()->index(ind, 1, testStr), value);
    }
    COMPARE_DATA(variableCollection()->index(len, 0, testStr), QString::number(len));
    COMPARE_DATA(variableCollection()->index(len, 1, testStr), "0 '\\000'");

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesWatchesTwoSessions()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateWatches);

    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    variableCollection()->watches()->add("ts");
    WAIT_FOR_A_WHILE(session, 300);

    QModelIndex ts = variableCollection()->index(0, 0, variableCollection()->index(0, 0));
    variableCollection()->expanded(ts);
    WAIT_FOR_A_WHILE(session, 100);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);

    //check if variable is marked as out-of-scope
    QCOMPARE(variableCollection()->watches()->childCount(), 1);
    KDevelop::Variable* v = watchVariableAt(0);
    QVERIFY(v);
    QVERIFY(!v->inScope());

    // TODO: not sure what this means, reenable this after fix access problem
    /*
    QCOMPARE(v->childCount(), 3);
    v = dynamic_cast<KDevelop::Variable*>(v->child(0));
    QVERIFY(!v->inScope());
    */

    //start a second debug session
    session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateWatches);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 300);

    QCOMPARE(variableCollection()->watches()->childCount(), 1);
    ts = variableCollection()->index(0, 0, variableCollection()->index(0, 0));
    v = watchVariableAt(0);
    QVERIFY(v);
    QVERIFY(v->inScope());

    // TODO: not sure what this means, reenable this after fix access problem
    /*
    QCOMPARE(v->childCount(), 3);
    v = dynamic_cast<KDevelop::Variable*>(v->child(0));
    QVERIFY(v->inScope());
    QCOMPARE(v->data(1, Qt::DisplayRole).toString(), QString::number(0));
    */

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);

    //check if variable is marked as out-of-scope
    // TODO: not sure what this means, reenable this after fix access problem
    /*
    v = dynamic_cast<KDevelop::Variable*>(variableCollection()->watches()->child(0));
    QVERIFY(!v->inScope());
    QVERIFY(!dynamic_cast<KDevelop::Variable*>(v->child(0))->inScope());
    */
}

void LldbTest::testVariablesStopDebugger()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateLocals);

    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    session->stopDebugger();
    WAIT_FOR_A_WHILE(session, 300);
}


void LldbTest::testVariablesStartSecondSession()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateLocals);

    TestLaunchConfiguration cfg;

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    session = new TestDebugSession;
    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateLocals);

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 38);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);

    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesSwitchFrame()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateLocals);
    TestFrameStackModel *stackModel = session->frameStackModel();

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 500);

    QModelIndex i = variableCollection()->index(1, 0);
    COMPARE_DATA(i, "Locals");
    QCOMPARE(variableCollection()->rowCount(i), 1);
    COMPARE_DATA(variableCollection()->index(0, 0, i), "i");
    COMPARE_DATA(variableCollection()->index(0, 1, i), "1");

    stackModel->setCurrentFrame(1);
    WAIT_FOR_A_WHILE(session, 200);

    i = variableCollection()->index(1, 0);
    QCOMPARE(variableCollection()->rowCount(i), 4);
    COMPARE_DATA(variableCollection()->index(2, 0, i), "argc");
    COMPARE_DATA(variableCollection()->index(2, 1, i), "1");
    COMPARE_DATA(variableCollection()->index(3, 0, i), "argv");

    breakpoints()->removeRow(0);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

void LldbTest::testVariablesQuicklySwitchFrame()
{
    QSKIP("TODO");
    TestDebugSession *session = new TestDebugSession;
    TestLaunchConfiguration cfg;

    session->variableController()->setAutoUpdate(KDevelop::IVariableController::UpdateLocals);
    TestFrameStackModel *stackModel = session->frameStackModel();

    breakpoints()->addCodeBreakpoint(QUrl::fromLocalFile(m_debugeeFileName), 24);
    QVERIFY(session->startDebugging(&cfg, m_iface));
    WAIT_FOR_STATE(session, DebugSession::PausedState);
    WAIT_FOR_A_WHILE(session, 500);

    QModelIndex i = variableCollection()->index(1, 0);
    COMPARE_DATA(i, "Locals");
    QCOMPARE(variableCollection()->rowCount(i), 1);
    COMPARE_DATA(variableCollection()->index(0, 0, i), "i");
    COMPARE_DATA(variableCollection()->index(0, 1, i), "1");

    stackModel->setCurrentFrame(1);
    WAIT_FOR_A_WHILE(session, 300);
    stackModel->setCurrentFrame(0);
    WAIT_FOR_A_WHILE(session, 1);
    stackModel->setCurrentFrame(1);
    WAIT_FOR_A_WHILE(session, 1);
    stackModel->setCurrentFrame(0);
    WAIT_FOR_A_WHILE(session, 1);
    stackModel->setCurrentFrame(1);
    WAIT_FOR_A_WHILE(session, 500);

    i = variableCollection()->index(1, 0);
    QCOMPARE(variableCollection()->rowCount(i), 4);
    QStringList locs;
    for (int j = 0; j < variableCollection()->rowCount(i); ++j) {
        locs << variableCollection()->index(j, 0, i).data().toString();
    }
    QVERIFY(locs.contains("argc"));
    QVERIFY(locs.contains("argv"));
    QVERIFY(locs.contains("x"));

    breakpoints()->removeRow(0);
    session->run();
    WAIT_FOR_STATE(session, DebugSession::EndedState);
}

QTEST_MAIN(KDevMI::LLDB::LldbTest);

#include "test_lldb.moc"
