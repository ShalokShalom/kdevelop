/***************************************************************************
                             doctreewidget.h
                             -------------------
    copyright            : (C) 1999 by Bernd Gehrmann
    email                : bernd@physik.hu-berlin.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#ifndef _DOCTREEWIDGET_H_
#define _DOCTREEWIDGET_H_

#include "klistview.h"


class DocTreeKDevelopFolder;
class DocTreeKDELibsFolder;
class DocTreeOthersFolder;
class DocTreeProjectFolder;
class DocTreeDocbaseFolder;
class DocTreeView;
class CProject;
class CustomizeDialog;


class DocTreeWidget : public KListView
{
    Q_OBJECT
    
public: 
    DocTreeWidget(DocTreeView *view);
    ~DocTreeWidget();

    void configurationChanged();
    //    void createConfigWidget(CustomizeDialog *dlg);
    void docPathChanged();
    void projectClosed();
    void projectOpened(CProject *prj);

private slots:
    void slotConfigure();
    void slotItemExecuted(QListViewItem *item);
    void slotRightButtonPressed(QListViewItem *item, const QPoint &p, int);
	
private: 
    QListViewItem *contextItem;
    DocTreeKDevelopFolder *folder_kdevelop;
    DocTreeKDELibsFolder *folder_kdelibs;
    DocTreeOthersFolder *folder_others;
    DocTreeDocbaseFolder *folder_docbase;
    DocTreeProjectFolder *folder_project;
    DocTreeView *m_view;
};
#endif
