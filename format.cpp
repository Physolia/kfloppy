/*

    $Id$

    Copyright (C) 2002 Adriaan de Groot
                       groot@kde.org


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    */

#include "debug.h"

#include <unistd.h>
#include <stdlib.h>

#include <qptrlist.h>
#include <qfile.h>
#include <qtimer.h>

#include <klocale.h>
#include <kprocess.h>
#include <kstandarddirs.h>

#include "format.moc"

static QString extPath = QString::null;

/* static */ QString findExecutable(const QString &e)
{
	if (extPath.isEmpty())
	{
		QString path = getenv("PATH");
		if (!path.isEmpty()) path.append(":");
		path.append("/usr/sbin:/sbin");
		extPath = path;
	}
		
	return KGlobal::dirs()->findExe(e, extPath);
}



KFAction::KFAction(QObject *parent) :
	QObject(parent)
{
	DEBUGSETUP;
}

KFAction::~KFAction()
{
	DEBUGSETUP;
	quit();
}

/* slot */ void KFAction::quit()
{
	DEBUGSETUP;
}

/* slot */ void KFAction::exec()
{
	DEBUGSETUP;
}

class KFActionQueue_p
{
public:
	QPtrList<KFAction> list;
} ;

KFActionQueue::KFActionQueue(QObject *parent) :
	KFAction(parent),
	d(new KFActionQueue_p)
{
	DEBUGSETUP;
	d->list.setAutoDelete(true);
}

KFActionQueue::~KFActionQueue()
{
	DEBUGSETUP;
	delete d;
}

void KFActionQueue::queue(KFAction *p)
{
	DEBUGSETUP;
	
	d->list.append(p);
	DEBUGS(p->name());
}

/* virtual */ void KFActionQueue::exec()
{
	DEBUGSETUP;
	
	actionDone(0L,true);
}

/* slot */ void KFActionQueue::actionDone(KFAction *p,bool success)
{
	DEBUGSETUP;
	
	if (p)
	{
		if (d->list.first()==p)
		{
			d->list.removeFirst();
			// delete p;           /* auto-delete */
		}
		else
		{
			DEBUGS(  "Strange pointer received.");
			emit done(this,false);
			return;
		}
	}
	else
	{
		DEBUGS("Starting action queue.");
	}
	
	if (!success)
	{
		DEBUGS("Action failed.");
		emit done(this,false);
		return;
	}
	
	KFAction *next = d->list.first();
	if (!next)
	{
		emit done(this,true);
	}
	else
	{
		DEBUGS("  Running action " << d->name());
		QObject::connect(next,SIGNAL(done(KFAction *,bool)),
			this,SLOT(actionDone(KFAction *,bool)));
		// Propagate signals
		QObject::connect(next,SIGNAL(status(const QString &,int)),
			this,SIGNAL(status(const QString &,int)));
		QTimer::singleShot(0,next,SLOT(exec()));
	}
}




// Here we have names of devices. The variable
// names are basically the linux device names,
// replace with whatever your OS needs instead.
//
//
#ifdef ANY_LINUX
const char *fd0H1440[] = { "/dev/fd0H1440", "/dev/fd0u1440", 0L } ;
const char *fd0D720[]={ "/dev/fd0D720", "/dev/fd0u720", 0L };
const char *fd0h1200[]={ "/dev/fd0h1200", 0L };
const char *fd0h360[]={ "/dev/fd0h360", 0L };
const char *fd1H1440[] = { "/dev/fd1H1440", "/dev/fd0u1440", 0L } ;
const char *fd1D720[]={ "/dev/fd1D720", "/dev/fd0u720", 0L };
const char *fd1h1200[]={ "/dev/fd1h1200", 0L };
const char *fd1h360[]={ "/dev/fd1h360", 0L };
#endif

#ifdef ANY_BSD
const char *fd0[] = { "/dev/fd0", 0L } ;
const char *fd1[] = { "/dev/fd1", 0L } ;
#endif

// Next we have a table of device names and characteristics.
// These are ordered according to 2*densityIndex+deviceIndex,
// ie. primary (0) 1440K (0) is first, then secondary (1) 1440K is
// second, down to secondary (1) 360k (4) in position 3*2+1=7.
//
//
// Note that the data originally contained in KFloppy was
// patently false, so most of this is fake. I guess noone ever
// formatted a 5.25" floppy.
//
// The flags field is unused in this implementation.
//
//
fdinfo fdtable[] =
{
#ifdef ANY_LINUX
        // device  drv blks trk flg
	{ fd0H1440, 0, 1440, 80, 0 },
	{ fd1H1440, 1, 1440, 80, 0 },
	{ fd0D720,  0,  720, 80, 0 },
	{ fd1D720,  1,  720, 80, 0 },
	{ fd0h1200, 0, 1200, 80, 0 },
	{ fd1h1200, 1, 1200, 80, 0 },
	{ fd0h360,  0,  360, 80, 0 },
	{ fd1h360,  1,  360, 80, 0 },
#endif

#ifdef ANY_BSD
	// Instead of the number of tracks, which is
	// unneeded, we record the
	// number of F's printed during an fdformat
	{ fd0, 0, 1440, 40, 0 },	
	{ fd1, 1, 1440, 40, 0 },
	{ fd0, 0,  720, 40, 0 },
	{ fd1, 1,  720, 40, 0 },
	{ fd0, 0, 1200, 40, 0},
	{ fd1, 1, 1200, 40, 0},
	{ fd0, 0,  360, 40, 0},
	{ fd1, 1,  360, 40, 0},
#endif
	{ 0L, 0, 0, 0, 0 }
} ;


FloppyAction::FloppyAction(QObject *p) :
	KFAction(p),
	deviceInfo(0L),
	deviceName(0L),
	theProcess(0L)
{
	DEBUGSETUP;
}

void FloppyAction::quit()
{
	DEBUGSETUP;
	if (theProcess)
	{
		delete theProcess;
		theProcess=0L;
	}

	KFAction::quit();
}

bool FloppyAction::configureDevice(int drive,int density)
{
	DEBUGSETUP;
	const char *devicename = 0L;
	
	deviceInfo=0L;
	deviceName=0L;
		
	if ((drive<0) || (drive>1))
	{
		emit status(i18n("Unexpected drive number %1.").arg(drive),-1);
		return false;
	}
	if (!( /* (2880==de)  || */ (1440==density) || (720==density) ||
		(1200==density) || (360==density)))
	{
		emit status(i18n("Unexpected density number %1.").arg(density),-1);
		return false;
	}

	fdinfo *deviceinfo = fdtable;
	for ( ; deviceinfo && (deviceinfo->devices) ; deviceinfo++)
	{
		if ((deviceinfo->blocks == density) && (deviceinfo->drive == drive)) break;
	}
	
	if (!deviceinfo->devices)
	{
		emit status(i18n("Cannot find a device for drive %1 and density %2.")
			.arg(drive).arg(density),-1);
		return false;
	}
	
	for (const char **devices=deviceinfo->devices ;
		*devices ; devices++)
	{
		if (access(*devices,W_OK)>=0)
		{
			DEBUGS(QString("  Found device %1").arg(*devices).latin1());
			devicename=*devices;
			break;
		}
	}

	if (!devicename)
	{
		QString str = i18n(
			"Cannot access %1\nMake sure that the device exists and that "
			"you have write permission to it.").arg(deviceinfo->devices[0]);
		emit status(str,-1);
		return false;
	}
	
	deviceName = devicename;
	deviceInfo = deviceinfo;
	
	return true;
}
	
void FloppyAction::processDone(KProcess *p)
{
	DEBUGSETUP;
	
	if (p!=theProcess)
	{
		DEBUGS("  Strange process exited.");
		return;
	}
	
	if (p->normalExit())
	{
		emit status(QString::null,100);
		emit done(this,!p->exitStatus());
	}
	else
	{
		emit status(i18n("%1 terminated abnormally.").arg(theProcessName),100);
		emit done(this,false);
	}
}

void FloppyAction::processStdOut(KProcess *, char *b, int l)
{
	DEBUGS("stdout:" << QString::fromLatin1(b,l));
}

void FloppyAction::processStdErr(KProcess *p, char *b, int l)
{
	processStdOut(p,b,l);
}

bool FloppyAction::startProcess()
{
	DEBUGSETUP;

	connect(theProcess,SIGNAL(processExited(KProcess *)),
		this,SLOT(processDone(KProcess *)));
	connect(theProcess,SIGNAL(receivedStdout(KProcess *,char *,int)),
		this,SLOT(processStdOut(KProcess *,char *,int)));
	connect(theProcess,SIGNAL(receivedStderr(KProcess *,char *,int)),
		this,SLOT(processStdErr(KProcess *,char *,int)));
		
		
	return theProcess->start(KProcess::NotifyOnExit,
		KProcess::AllOutput);
}


/* static */ QString FDFormat::fdformatName = QString::null;

FDFormat::FDFormat(QObject *p) : 
	FloppyAction(p),
	doVerify(true)
{
	DEBUGSETUP;
	theProcessName = QString::fromLatin1("fdformat");
	setName("FDFormat");
}

/* static */ bool FDFormat::runtimeCheck()
{
	fdformatName = findExecutable("fdformat");
	return (!fdformatName.isEmpty());
}

bool FDFormat::configure(bool v)
{
	doVerify=v;
	return true;
}

/* virtual */ void FDFormat::exec()
{
	DEBUGSETUP;
	
	if (!deviceInfo || !deviceName)
	{
		emit done(this,false);
		return;
	}
	
	if (fdformatName.isEmpty())
	{
		emit status(i18n("Cannot find fdformat."),-1);
		emit done(this,false);
		return;
	}
	
	if (theProcess) delete theProcess;
	theProcess = new KProcess;


	formatTrackCount=0;

	*theProcess << fdformatName ;
	
	// Common to Linux and BSD, others may differ
	if (!doVerify)
	{
		*theProcess << "-n";
	}
	
#ifdef ANY_BSD
	*theProcess
		<< "-y"
		<< "-f"
		<< QString::number(deviceInfo->blocks) ;
#else
#ifdef ANY_LINUX
	// No Linux-specific flags
#endif
#endif

	// Common to Linux and BSD, others may differ
	*theProcess << deviceName ;

	if (!startProcess())
	{
		emit status(i18n("Could not start fdformat."),-1);
		emit done(this,false);
	}
	
	// Now depend on fdformat running and producing output.
}

// Parse some output from the fdformat process. Lots of
// #ifdefs here to account for variations in the basic
// fdformat. Uses gotos to branch to whatever error message we
// need, since the messages can be standardized across OSsen.
//
//
void FDFormat::processStdOut(KProcess *, char *b, int l)
{
	DEBUGSETUP;
	QString s;

#ifdef ANY_BSD
	if (b[0]=='F')
	{
		formatTrackCount++;
		emit status(QString::null,
			formatTrackCount * 100 / deviceInfo->tracks);
	}
	else if (b[0]=='E')
	{
		emit status(i18n("Error formatting track %1.").arg(formatTrackCount),-1);
	}
	else
	{
		s = QString::fromLatin1(b,l);
		if (s.contains("ioctl(FD_FORM)"))
		{
			goto ioError;
		}
		if (s.find("/dev/")>=0)
		{
			emit status(s,-1);
			return;
		}
		DEBUGS(s.latin1());
	}
#else
#ifdef ANY_LINUX
	s = QString::fromLatin1(b,l);
	int p;
	if ((p=s.find("track")) != -1)
	{
		p+=5;
		while ((0<=p) && (p<l) && (s[p].isSpace())) p++;
		if (s[p].isDigit())
		{
			p=s.mid(p,8).toInt();
			if ((p>0) && (p<deviceInfo->tracks))
			{
				emit status(QString::null,
					p * 100 / deviceInfo->tracks);
			}
		}
	}
	else if (s.contains("ioctl(FDFMTBEG)"))
	{
		goto ioError;
	}
	DEBUGS(s.latin1());
#endif
#endif
	return;

// Now a bunch of (goto) labels for all the possible error messages.
//
//
ioError:
	emit status (i18n(
		"Cannot access floppy or floppy drive.\n"
		"Please insert a floppy and make sure that you "
		"have selected a valid floppy drive."),-1);
	return;
}



/* static */ QString FATFilesystem::newfs_fat = QString::null ;

FATFilesystem::FATFilesystem(QObject *parent) :
	FloppyAction(parent)
{
	DEBUGSETUP;
	runtimeCheck();
	theProcessName=newfs_fat;
	setName("FATFilesystem");
}

/* static */ bool FATFilesystem::runtimeCheck()
{
	DEBUGSETUP;
	
#ifdef ANY_BSD
	newfs_fat = findExecutable("newfs_msdos");
#else
#ifdef ANY_LINUX
	newfs_fat = findExecutable("mkdosfs");
#else
	return false;
#endif
#endif

	return !newfs_fat.isEmpty();
}

bool FATFilesystem::configure(bool v,bool l,const QString &lbl)
{
	doVerify=v;
	doLabel=l;
	if (l)
	{
		label=lbl.stripWhiteSpace();
	}
	else
	{
		label=QString::null;
	}

	return true;
}

void FATFilesystem::exec()
{
	DEBUGSETUP;

	if (!deviceInfo || !deviceName)
	{
		emit done(this,false);
		return;
	}
	
	if (newfs_fat.isEmpty())
	{
		emit status(i18n("Cannot find a program to create FAT filesystems."),-1);
		emit done(this,false);
		return;
	}

	if (theProcess) delete theProcess;
	KProcess *p = theProcess = new KProcess;	
	
	*p << newfs_fat;
#ifdef ANY_BSD
	*p << "-f" << QString::number(deviceInfo->blocks);
	if (doLabel)
	{
		*p << "-L" << label ;
	}
#else
#ifdef ANY_LINUX
	if (doLabel)
	{
		*p << "-n" << label ;
	}
	if (doVerify)
	{
		*p << "-c";
	}
#endif
#endif
	*p << deviceName ;

	if (!startProcess())
	{
		emit status(i18n("Cannot start FAT format program."),-1);
		emit done(this,false);
	}
}






#ifdef ANY_BSD

/* static */ QString UFSFilesystem::newfs = QString::null ;

UFSFilesystem::UFSFilesystem(QObject *parent) :
	FloppyAction(parent)
{
	DEBUGSETUP;
	runtimeCheck();
	theProcessName=newfs;
	setName("UFSFilesystem");
}

/* static */ bool UFSFilesystem::runtimeCheck()
{
	DEBUGSETUP;
	
	newfs = findExecutable("newfs");

	return !newfs.isEmpty();
}

void UFSFilesystem::exec()
{
	DEBUGSETUP;

	if (!deviceInfo || !deviceName)
	{
		emit done(this,false);
		return;
	}
	
	if (newfs.isEmpty())
	{
		emit status(i18n("Cannot find a program to create a UFS filesystems."),-1);
		emit done(this,false);
		return;
	}

	if (theProcess) delete theProcess;
	KProcess *p = theProcess = new KProcess;	
	
	*p << newfs;
	*p << "-T" << QString("fd%1").arg(deviceInfo->blocks)
		<< deviceName;

	if (!startProcess())
	{
		emit status(i18n("Cannot start UFS format program."),-1);
		emit done(this,false);
	}
}
#endif



#ifdef ANY_LINUX
/* static */ QString Ext2Filesystem::newfs = QString::null ;

Ext2Filesystem::Ext2Filesystem(QObject *parent) :
	FloppyAction(parent)
{
	DEBUGSETUP;
	runtimeCheck();
	theProcessName="mke2fs";
	setName("Ext2Filesystem");
}

/* static */ bool Ext2Filesystem::runtimeCheck()
{
	DEBUGSETUP;
	
	newfs = findExecutable("mke2fs");

	return !newfs.isEmpty();
}

bool Ext2Filesystem::configure(bool v,bool l,const QString &lbl)
{
	doVerify=v;
	doLabel=l;
	if (l)
	{
		label=lbl.stripWhiteSpace();
	}
	else
	{
		label=QString::null;
	}

	return true;
}

void Ext2Filesystem::exec()
{
	DEBUGSETUP;

	if (!deviceInfo || !deviceName)
	{
		emit done(this,false);
		return;
	}
	
	if (newfs.isEmpty())
	{
		emit status(i18n("Cannot find a program to create an ext2 filesystems."),-1);
		emit done(this,false);
		return;
	}

	if (theProcess) delete theProcess;
	KProcess *p = theProcess = new KProcess;	
	
	*p << newfs;
	*p << "-q";
	if (doVerify) *p << "-c" ;
	if (doLabel) *p << "-L" << label ;

	if (!startProcess())
	{
		emit status(i18n("Cannot start ext2 format program."),-1);
		emit done(this,false);
	}
}



#endif
