/**
 * The interface to a app frontend
 */

#ifndef _KDEVAPPFRONTEND_H_
#define _KDEVAPPFRONTEND_H_

#include <qstringlist.h>
#include "kdevplugin.h"


class KDevAppFrontend : public KDevPlugin
{
    Q_OBJECT

public:

    KDevAppFrontend( const QString& pluginName, const QString& icon, QObject *parent=0, const char *name=0 );
    ~KDevAppFrontend();

    /**
     * Returns whether the application is currently running.
     */
    virtual bool isRunning() = 0;

public slots:
    /**
     * The component shall start to execute an app-like command.
     * Running the application is always asynchronous.
     * If directory is empty it will use the user's home directory.
     * If inTerminal is true, the program is started in an external
     * konsole.
     */
    virtual void startAppCommand(const QString &directory, const QString &program, bool inTerminal) = 0;
    /**
     * Stop the currently running application
     */
    virtual void stopApplication() = 0;
    /**
     * Inserts a string into the view.
     */
    virtual void insertStdoutLine(const QString &line) = 0;
    /**
     * Inserts a string into the view marked as stderr output
     * (colored in the current implementation).
     */
    virtual void insertStderrLine(const QString &line) = 0;
};

#endif
