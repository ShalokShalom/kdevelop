/**
	 Copyright (C) 2003-2005 Mickael Marchand <marchand@kde.org>

	 This program is free software; you can redistribute it and/or
	 modify it under the terms of the GNU General Public
	 License as published by the Free Software Foundation; either
	 version 2 of the License, or (at your option) any later version.

	 This program is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with this program; see the file COPYING.  If not, write to
	 the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
	 Boston, MA 02111-1307, USA.
	 */

#include <kparts/part.h>
#include <kdevcore.h>
#include "subversion_part.h"
#include "subversion_core.h"
#include "subversion_widget.h"
#include "svn_blamewidget.h"
#include "svn_logviewwidget.h"
#include "subversiondiff.h"
#include <kdevmainwindow.h>
#include "svn_co.h"
#include <kurlrequester.h>
#include <klineedit.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kio/netaccess.h>
#include <kdebug.h>
#include <kmainwindow.h>
#include <kapplication.h>
#include <dcopclient.h>
#include <ktempfile.h>
#include <kprocess.h>
#include <kstandarddirs.h>
#include <qtextcodec.h>
#include <qtextstream.h>
#include <qtextbrowser.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <qregexp.h>

#include <kapplication.h>
#include <kinstance.h>
#include <kaboutdata.h>

using namespace KIO;

subversionCore::subversionCore(subversionPart *part)
// 	: QObject(NULL, "subversion core"), DCOPObject("subversion") {
	: QObject(NULL, "subversion core") {
		m_part = part;
// 		m_widget = new subversionWidget(part, 0 , "subversionprocesswidget");
		m_logViewWidget = new SvnLogViewWidget( part, 0 );
// 		m_part->mainWindow()->embedOutputView( m_logViewWidget, i18n( "Subversion Log" ), i18n( "Subversion Log" ) );
//		if ( ! connectDCOPSignal("kded", "ksvnd", "subversionNotify(QString,int,int,QString,int,int,long int,QString)", "notification(QString,int,int,QString,int,int,long int,QString)", false ) )
//			kdWarning() << "Failed to connect to kded dcop signal ! Notifications won't work..." << endl;

        m_fileInfoProvider = new SVNFileInfoProvider( part );
		diffTmpDir = new KTempDir();
		diffTmpDir->setAutoDelete(true);
}

subversionCore::~subversionCore() {
// 	if ( processWidget() ) {
// 		m_part->mainWindow()->removeView( processWidget() );
// 		delete processWidget();
// 	}
	if( m_logViewWidget ){
		m_part->mainWindow()->removeView( m_logViewWidget );
		delete m_logViewWidget;
	}
	delete diffTmpDir;
	//FIXME delete m_fileInfoProvider here?
}

KDevVCSFileInfoProvider *subversionCore::fileInfoProvider() const {
    return m_fileInfoProvider;
}

//not used anymore
// void subversionCore::notification( const QString& path, int action, int kind, const QString& mime_type, int content_state ,int prop_state ,long int revision, const QString& userstring ) {
// 	kdDebug(9036) << "Subversion Notification : "
// 		<< "path : " << path
// 		<< "action: " << action
// 		<< "kind : " << kind
// 		<< "mime_type : " << mime_type
// 		<< "content_state : " << content_state
// 		<< "prop_state : " << prop_state
// 		<< "revision : " << revision
// 		<< "userstring : " << userstring
// 		<< endl;
// 	if ( !userstring.isEmpty() ) {
// 		m_part->mainWindow()->raiseView(processWidget());
// 		processWidget()->append( userstring );
// 	}
// }

//subversionWidget *subversionCore::processWidget() const {
SvnLogViewWidget* subversionCore::processWidget() const {
// 	return processWidget();
	return m_logViewWidget;
}

void subversionCore::resolve( const KURL::List& list ) {
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "resolving: " << (*it).prettyURL() << endl;
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		int cmd = 11;
		bool recurse = true;
		s << cmd << *it << recurse;
		SimpleJob * job = KIO::special(servURL, parms, true);
		job->setWindow( m_part->mainWindow()->main() );
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
	}
}

void subversionCore::update( const KURL::List& list ) {
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "updating : " << (*it).prettyURL() << endl;
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		int cmd = 2;
		int rev = -1;
		s << cmd << *it << rev << QString( "HEAD" );
		SimpleJob * job = KIO::special(servURL, parms, true);
		job->setWindow( m_part->mainWindow()->main() );
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
	}
}

void subversionCore::diff( const KURL::List& list, const QString& where){
	kdDebug(9036) << "diff " << list << endl;
	KURL servURL = "kdevsvn+svn://this_is_a_fake_URL_and_this_is_normal/";
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		int cmd = 13;
		kdDebug(9036) << "diffing : " << (*it).prettyURL() << endl;
		int rev1=-1;
		int rev2=-1;
		QString revkind1 = where;
		QString revkind2 = "WORKING";
		s << cmd << *it << *it << rev1 << revkind1 << rev2 << revkind2 << true ;
		KIO::SimpleJob * job = KIO::special(servURL, parms, true);
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
		KIO::NetAccess::synchronousRun( job, 0 );
		if ( diffresult.count() > 0 ) {
			//check kompare is available
			if ( !KStandardDirs::findExe( "kompare" ).isNull() ) {
				if (!KStandardDirs::findExe("patch").isNull()){
					// we have patch - so can merge
					KTempDir tmpDir = KTempDir(diffTmpDir->name());
					KTempFile tmpPatch = KTempFile(tmpDir.name());

					// write the patch
					QTextStream *stream = tmpPatch.textStream();
					stream->setCodec( QTextCodec::codecForName( "utf8" ) );
					for ( QStringList::Iterator it2 = diffresult.begin();it2 != diffresult.end() ; ++it2 ) {
						( *stream ) << ( *it2 ) << "\n";
					}
					tmpPatch.close();

					QString ourCopy = tmpDir.name()+(*it).fileName();

					KProcess copy;
					copy << "cp" << (*it).prettyURL(0,KURL::StripFileProtocol) <<  tmpDir.name();
					copy.start(KProcess::Block);

					KProcess patch;
					patch.setWorkingDirectory(tmpDir.name());
					patch << "patch" << "-R" << ourCopy << tmpPatch.name();
					patch.start(KProcess::Block, KProcess::All);

					KProcess *p = new KProcess;
					*p << "kompare" << ourCopy << (*it).prettyURL();
					p->start();
				}
				else{
					// only diff
					KTempFile *tmp = new KTempFile;
					tmp->setAutoDelete(true);
					QTextStream *stream = tmp->textStream();
					stream->setCodec( QTextCodec::codecForName( "utf8" ) );
					for ( QStringList::Iterator it2 = diffresult.begin();it2 != diffresult.end() ; ++it2 ) {
						( *stream ) << ( *it2 ) << "\n";
					}
					tmp->close();
					KProcess *p = new KProcess;
					*p << "kompare" << "-n" << "-o" << tmp->name();
					p->start();
				}
			} else { //else do it with message box
				Subversion_Diff df;
				for ( QStringList::Iterator it2 = diffresult.begin();it2 != diffresult.end() ; ++it2 ) {
					df.text->append( *it2 );
				}
				QFont f = df.font();
				f.setFixedPitch( true );
				df.text->setFont( f );
				df.exec();
			}
		}
		else{
			QString diffTo = i18n("the local disk checked out copy.");
			if ( where=="HEAD"){
				diffTo=i18n("the current svn HEAD version.");
			}
			KMessageBox::information( 0, i18n("No differences between the file and %1").arg(diffTo), i18n("No difference") );
		}
		diffresult.clear();
	}
}

void subversionCore::commit( const KURL::List& list, bool recurse, bool keeplocks ) {
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	QByteArray parms;
	QDataStream s( parms, IO_WriteOnly );
	int cmd = 103;
 	s << cmd << recurse << keeplocks;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "adding to list: " << (*it).prettyURL() << endl;
		s << *it;
	}
	SimpleJob * job = KIO::special(servURL, parms, true);
	job->setWindow( m_part->mainWindow()->main() );
	connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
}
// Right now, only one item for each action.
void subversionCore::svnLog( const KURL::List& list,
		int revstart, QString revKindStart, int revend, QString revKindEnd,
		bool repositLog, bool discorverChangedPath, bool strictNodeHistory )
{
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	QByteArray parms;
	QDataStream s( parms, IO_WriteOnly );
	// prepare arguments
	int cmd = 4;
// 	int revstart = -1, revend = 0;
// 	QString revKindStart = "HEAD", revKindEnd = "";
// 	bool repositLog = true, discorverChangedPath = true, strictNodeHistory = true;
	s << cmd << revstart << revKindStart << revend << revKindEnd;
	s << repositLog << discorverChangedPath << strictNodeHistory;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "svnCore: adding to list: " << (*it).prettyURL() << endl;
		s << *it;
	}
	SimpleJob * job = KIO::special(servURL, parms, true);
	job->setWindow( m_part->mainWindow()->main() );
	connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotLogResult( KIO::Job * ) ) );

}

void subversionCore::blame( const KURL &url, bool repositBlame, int revstart, QString revKindStart, int revend, QString revKindEnd )
{
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	QByteArray parms;
	QDataStream s( parms, IO_WriteOnly );
	// prepare arguments
	int cmd = 14;
	s << cmd << url << repositBlame ;
	s << revstart << revKindStart << revend << revKindEnd ;

	SimpleJob * job = KIO::special(servURL, parms, true);
	job->setWindow( m_part->mainWindow()->main() );
	connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotBlameResult( KIO::Job * ) ) );
}

void subversionCore::add( const KURL::List& list ) {
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "adding : " << (*it).prettyURL() << endl;
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		int cmd = 6;
		s << cmd << *it;
		SimpleJob * job = KIO::special(servURL, parms, true);
		job->setWindow( m_part->mainWindow()->main() );
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
	}
}

void subversionCore::del( const KURL::List& list ) {
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "deleting : " << (*it).prettyURL() << endl;
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		int cmd = 7;
		s << cmd << *it;
		SimpleJob * job = KIO::special(servURL, parms, true);
		job->setWindow( m_part->mainWindow()->main() );
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
	}
}

void subversionCore::revert( const KURL::List& list ) {
	KURL servURL = m_part->baseURL();
	if ( servURL.isEmpty() ) servURL="kdevsvn+svn://blah/";
	if ( ! servURL.protocol().startsWith( "kdevsvn+" ) ) {
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
	}
	kdDebug(9036) << "servURL : " << servURL.prettyURL() << endl;
	for ( QValueListConstIterator<KURL> it = list.begin(); it != list.end() ; ++it ) {
		kdDebug(9036) << "reverting : " << (*it).prettyURL() << endl;
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		int cmd = 8;
		s << cmd << *it;
		SimpleJob * job = KIO::special(servURL, parms, true);
		job->setWindow( m_part->mainWindow()->main() );
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotResult( KIO::Job * ) ) );
	}
}

void subversionCore::checkout() {
	svn_co checkoutDlg;

	if ( checkoutDlg.exec() == QDialog::Accepted ) {
		//checkout :)
		QByteArray parms;
		QDataStream s( parms, IO_WriteOnly );
		KURL servURL ( checkoutDlg.serverURL->url() );
		wcPath = checkoutDlg.localDir->url() + "/" + checkoutDlg.newDir->text();
		int cmd = 1;
		int rev = -1;
		s << cmd << servURL << KURL( wcPath ) << rev << QString( "HEAD" );
		servURL.setProtocol( "kdevsvn+" + servURL.protocol() ); //make sure it starts with "svn"
		SimpleJob * job = KIO::special(servURL,parms, true);
		job->setWindow( m_part->mainWindow()->main() );
		connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotEndCheckout( KIO::Job * ) ) );
	}
}

void subversionCore::slotEndCheckout( KIO::Job * job ) {
	if ( job->error() ) {
		job->showErrorDialog( m_part->mainWindow()->main() );
		emit checkoutFinished( QString::null );
	} else
		emit checkoutFinished(wcPath);
}

void subversionCore::slotResult( KIO::Job * job ) {
    if ( job->error() ){
        job->showErrorDialog( m_part->mainWindow()->main() );
        if( job->error() == ERR_CANNOT_LAUNCH_PROCESS )
            KMessageBox::error( m_part->mainWindow()->main(),
                                i18n("If you just have installed new version of KDevelop,"
                                     " and if the error message was unknown protocol kdevsvn+*,"
                                     " try to restart KDE"
                                    ) );
        return;
    }
    KIO::MetaData ma = job->metaData();
	QValueList<QString> keys = ma.keys();
	qHeapSort( keys );
	QValueList<QString>::Iterator begin = keys.begin(), end = keys.end(), it;

	for ( it = begin; it != end; ++it ) {
// 		kdDebug(9036) << "METADATA : " << *it << ":" << ma[ *it ] << endl;
		if ( ( *it ).endsWith( "string" ) ) {
			m_part->mainWindow()->raiseView(processWidget());
			processWidget()->append( ma[ *it ] );
		}
		//extra check to retrieve the diff output in case with run a diff command
		if ( ( *it ).endsWith( "diffresult" ) ) {
			diffresult << ma[ *it ];
		}
	}
}
void subversionCore::slotLogResult( KIO::Job * job )
{
	if ( job->error() ){
		job->showErrorDialog( m_part->mainWindow()->main() );
        if( job->error() == ERR_CANNOT_LAUNCH_PROCESS )
            KMessageBox::error( m_part->mainWindow()->main(),
                                i18n("If you just have installed new version of KDevelop,"
                                    " and if the error message was unknown protocol kdevsvn+*,"
                                    " try to restart KDE"
                                    ) );
		return;
	}

	holderList.clear();

	KIO::MetaData ma = job->metaData();
	QValueList<QString> keys = ma.keys();
	QRegExp rx( "([0-9]*)(.*)" );
	int curIdx, lastIdx;

	for (QValueList<QString>::Iterator it = keys.begin(); it != keys.end(); /*++it*/ ){
		if ( rx.search( *it ) == -1 ){
			kdDebug(9036) << " Exiting loop at line " << __LINE__ <<endl;
			return; // something is wrong ! :)
		}
		curIdx = lastIdx = rx.cap( 1 ).toInt();
		SvnLogHolder logHolder;
		while ( curIdx == lastIdx ) {
			kdDebug(9036) << "svn log MetaData: " << *it << ":" << ma[ *it ] << endl;

			if ( rx.cap( 2 ) == "author" )
				logHolder.author = ma[*it];
			else if ( rx.cap( 2	 ) == "date" )
				logHolder.date = ma[*it];
			else if ( rx.cap( 2	 ) == "logmsg" )
				logHolder.logMsg = ma[*it];
			else if ( rx.cap( 2	 ) == "pathlist" )
				logHolder.pathList = ma[*it];
			else if ( rx.cap( 2	 ) == "rev" )
				logHolder.rev = ma[*it];

			++it;
			if ( it == keys.end() )
				break;
			if ( rx.search( *it ) == -1 ){
				kdDebug(9036) << " Exiting loop at line " << __LINE__ <<endl;
				break; // something is wrong ! :)
			}
			curIdx = rx.cap( 1 ).toInt();
		}//end of while
		holderList.append( logHolder );
	}
	processWidget()->setLogResult( &holderList );
	m_part->mainWindow()->raiseView(processWidget());

}

void subversionCore::slotBlameResult( KIO::Job * job )
{
    if ( job->error() ){
        job->showErrorDialog( m_part->mainWindow()->main() );
        if( job->error() == ERR_CANNOT_LAUNCH_PROCESS )
            KMessageBox::error( m_part->mainWindow()->main(),
                                i18n("If you just have installed new version of KDevelop,"
                                     " and if the error message was unknown protocol kdevsvn+*,"
                                     " try to restart KDE"
                                    ) );
        return;
    }
	blameList.clear();

	KIO::MetaData ma = job->metaData();
	QValueList<QString> keys = ma.keys();
	QRegExp rx( "([0-9]*)(.*)" );
	int curIdx, lastIdx;

	for (QValueList<QString>::Iterator it = keys.begin(); it != keys.end(); /*++it*/ ){
		if ( rx.search( *it ) == -1 ){
			kdDebug(9036) << " Exiting loop at line " << __LINE__ <<endl;
			return; // something is wrong ! :)
		}

		// if metadata has action key, that means a notification for svn_wc_notify_blame_completed
		// Thus, consume this notification
		if ( rx.cap( 2 ) == "action" ){
			curIdx = lastIdx = rx.cap( 1 ).toInt();
			while ( curIdx == lastIdx ){
				++it;
				if ( it == keys.end() ) break;
				if ( rx.search( *it ) == -1 ) continue; // something is wrong
				curIdx = rx.cap( 1 ).toInt();
			}
			continue;
		}
		// get actual blame data
		curIdx = lastIdx = rx.cap( 1 ).toInt();
		SvnBlameHolder blameHolder;
		while ( curIdx == lastIdx ) {
			kdDebug(9036) << "svn blame MetaData: " << *it << ":" << ma[ *it ] << endl;

			if ( rx.cap( 2 ) == "LINE" )
				blameHolder.line= (ma[*it]).toInt();
			else if ( rx.cap( 2	 ) == "REV" )
				blameHolder.rev = (ma[*it]).toLongLong();
			else if ( rx.cap( 2	 ) == "AUTHOR" )
				blameHolder.author= ma[*it];
			else if ( rx.cap( 2	 ) == "DATE" )
				blameHolder.date= ma[*it];
			else if ( rx.cap( 2	 ) == "CONTENT" )
				blameHolder.content = ma[*it];

			++it;
			if ( it == keys.end() )
				break;
			if ( rx.search( *it ) == -1 ){
				kdDebug(9036) << " Exiting loop at line " << __LINE__ <<endl;
				break; // something is wrong ! :)
			}
			curIdx = rx.cap( 1 ).toInt();
		}//end of while
		blameList.append( blameHolder );
// 		blameList.insert( blameHolder.line, blameHolder );
	}
	SvnBlameWidget dlg;
	dlg.copyBlameData( &blameList );
	dlg.exec();
}

void subversionCore::createNewProject( const QString& // dirName
                                       , const KURL& // importURL
                                       , bool // init
                                       ) {

}

#include "subversion_core.moc"
