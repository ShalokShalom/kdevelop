/***************************************************************************
                          ClassStore.h  -  description
                             -------------------
    begin                : Fri Mar 19 1999
    copyright            : (C) 1999 by Jonas Nordin
    email                : jonas.nordin@cenacle.se
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#ifndef _CLASSTORE_H_INCLUDED
#define _CLASSTORE_H_INCLUDED

#include "ParsedClass.h"
#include "PersistantClassStore.h"
#include "ParsedStruct.h"
#include "ClassTreeNode.h"

/** This class has the ability to store and fetch parsed items. 
 * @author Jonas Nordin(jonas.nordin@cenacle.se)
 */
class CClassStore
{
public: // Constructor & Destructor

  CClassStore();
  ~CClassStore();

private: // Private attributes

  /** Store for global pre-parsed classes(like Qt and KDE). */
  CPersistantClassStore globalStore;

  /** All parsed classes. */
  QDict<CParsedClass> classes;

  /** Maps a filename to a number of classnames. **/
  QDict<QStrList> filenameMap;

public: // Public attributes

  /** Iterator for the classes */
  QDictIterator<CParsedClass> classIterator;

  /** Container that holds all functions, variables and structures. */
  CParsedContainer globalContainer;

public: // Public queries

  /** Return the store as a forest(collection of trees). */
  QList<CClassTreeNode> *asForest();

  /** Tells if a class exists in the store. 
   * @param aName Classname to check if it exists.
   */
  bool hasClass( const char *aName );

  /** Fetches a class from the store by using its' name. */
  CParsedClass *getClassByName( const char *aName );

  /** Fetches all classes with the named parent. */
  QList<CParsedClass> *getClassesByParent( const char *aName );

  /** Fetches all clients of a named class. */
  QList<CParsedClass> *getClassClients( const char *aName );

  /** Fetches all suppliers of a named class. */
  QList<CParsedClass> *getClassSuppliers( const char *aName );

  /** Get all classes in sorted order. */
  QList<CParsedClass> *getSortedClasslist();

  /** Fetch all virtual methods, both implemented and not.
   * @param aName The class to fetch virtual methods for
   * @param implList The list that will contain the 
   *  implemented virtual methods.
   * @param availList The list hat will contain the available virtual
   *  methods. */
  void getVirtualMethodsForClass( const char *aName, 
                                  QList<CParsedMethod> *implList,
                                  QList<CParsedMethod> *availList );

public: // Public Methods

  /** Remove all parsed classes. */
  void wipeout();

  /** Add a classdefintion. */
  void addClass( CParsedClass *aClass );

  /** Add a global variable. */
  void addGlobalVar( CParsedAttribute *aAttr );

  /** Add a global function. */
  void addGlobalFunction( CParsedMethod *aFunc );

  /** Add a global structure. */
  void addGlobalStruct( CParsedStruct *aStruct );

  /** Remove all items in the store with references to the file. 
   * @param aFile The file to check references to.
   */
  void removeWithReferences( const char *aFile );

  /** Remove a class from the store. 
   * @param aName Name of the class to remove
   */
  void removeClass( const char *aName );

  /** Store all parsed classes as a database. */
  void storeAll();

  /** Output this object as text on stdout */
  void out();

};

#endif
