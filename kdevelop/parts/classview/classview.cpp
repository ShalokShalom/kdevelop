#include <qlcdnumber.h>
#include <qpopupmenu.h>
#include <kdebug.h>
#include <klocale.h>
#include <kaction.h>
#include "classview.h"
#include "classactions.h"
#include "main.h"
#include "cproject.h"
#include "ctreehandler.h"
#include "ClassStore.h"
#include "caddclassmethoddlg.h"
#include "caddclassattributedlg.h"


ClassView::ClassView(QObject *parent, const char *name)
    : KDevComponent(parent, name)
{
    setInstance(ClassFactory::instance());
    setXMLFile("kdevclassview.rc");
    
    m_cv_decl_or_impl = false;
    m_store = 0;
}


ClassView::~ClassView()
{}


void ClassView::setupGUI()
{
    QLCDNumber *w = new QLCDNumber();
    w->display(42);
    //    w->setIcon()
    w->setCaption(i18n("Class view"));
    embedWidget(w, SelectView, i18n("CV"));
    
    classes_action = new ClassListAction(i18n("Classes"), 0, this, SLOT(selectedClass()),
                                         actionCollection(), "class_combo");
    methods_action = new MethodListAction(i18n("Methods"), 0, this, SLOT(selectedMethod()),
                                          actionCollection(), "method_combo");
    popup_action  = new KDevDelayedPopupAction(i18n("Declaration/Implementation"), "classwiz", 0, this, SLOT(switchedDeclImpl()),
                                               actionCollection(), "class_wizard");
    popup_action->popupMenu()->insertItem(i18n("Goto declaration"), this, SLOT(selectedGotoDeclaration()));
    popup_action->popupMenu()->insertItem(i18n("Goto implementation"), this, SLOT(selectedGotoImplementation()));
    popup_action->popupMenu()->insertItem(i18n("Goto class declaration"), this, SLOT(selectedGotoClassDeclaration()));
    popup_action->popupMenu()->insertSeparator();
    popup_action->popupMenu()->insertItem(i18n("Add method..."), this, SLOT(selectedAddMethod()));
    popup_action->popupMenu()->insertItem(i18n("Add attribute..."), this, SLOT(selectedAddAttribute()));
}


void ClassView::projectOpened(CProject *prj)
{
    kdDebug(9003) << "ClassView::projectOpened()" << endl;
    //    classWidget()->refresh(prj);
}

void ClassView::projectClosed()
{
    kdDebug(9003) << "ClassView::projectClosed()" << endl;
    //    classWidget()->clear();
}


void ClassView::classStoreOpened(CClassStore *store)
{
    kdDebug(9003) << "ClassView::classStoreOpened()" << endl;
    classes_action->setClassStore(store);
    methods_action->setClassStore(store);
    classes_action->refresh();
    classes_action->refresh();
    m_store = store;
}


void ClassView::classStoreClosed()
{
    kdDebug(9003) << "ClassView::classStoreClosed()" << endl;
    classes_action->setClassStore(0);
    methods_action->setClassStore(0);
    classes_action->refresh();
    classes_action->refresh();
    m_store = 0;
}


void ClassView::addedFileToProject(const QString &name)
{
    kdDebug(9003) << "ClassView::addedFileToProject()" << endl;
    // This could be much finer-grained...
    //    classWidget()->refresh();
}


void ClassView::removedFileFromProject(const QString &name)
{
    kdDebug(9003) << "ClassView::removedFileFromProject()" << endl;
    // This could be much finer-grained...
    //    classWidget()->refresh();
}


void ClassView::savedFile(const QString &name)
{
    kdDebug(9003) << "ClassView::savedFile()" << endl;
    if (CProject::getType(name) == CPP_HEADER)
        ;
        //        classWidget()->refresh();
}


/**
 * The user selected a class in the toolbar class combo.
 */
void ClassView::selectedClass()
{
    QString className = classes_action->currentText();
    if (className.isEmpty())
        return;
    
    kdDebug(9003) << "ClassView: Class selected: " << className << endl;
    methods_action->refresh(className);
}


/**
 * The user selected a method in the toolbar method combo.
 */
void ClassView::selectedMethod()
{
    QString className = classes_action->currentText();
    QString methodName = methods_action->currentText();
    if (className.isEmpty() || methodName.isEmpty())
        return;

    kdDebug(9003) << "ClassView: Method selected: "
                  << className << "::" << methodName << endl;
    m_cv_decl_or_impl = true;
    gotoImplementation(className, methodName, THPUBLIC_METHOD);
}


/**
 * The user clicked on the class wizard button.
 */
void ClassView::switchedDeclImpl()
{
    QString className = classes_action->currentText();
    QString methodName = methods_action->currentText();

    kdDebug(9003) << "ClassView::switchedDeclImpl" << endl;
    if (m_cv_decl_or_impl) {
        m_cv_decl_or_impl = false;
        gotoDeclaration(className, methodName, methodName.isEmpty()? THCLASS : THPUBLIC_METHOD);
    } else {
        m_cv_decl_or_impl = true;
        if (methodName.isEmpty())
            gotoDeclaration(className, "", THCLASS);
        else
            gotoImplementation(className, methodName, THPUBLIC_METHOD);
    }
}


/**
 * The user selected "Goto declaration" from the delayed class wizard popup.
 */
void ClassView::selectedGotoDeclaration()
{
    QString className = classes_action->currentText();
    QString methodName = methods_action->currentText();
    
    gotoDeclaration(className, methodName, methodName.isEmpty()? THCLASS : THPUBLIC_METHOD);
}


/**
 * The user selected "Goto class declaration" from the delayed class wizard popup.
 */
void ClassView::selectedGotoClassDeclaration()
{
    QString className = classes_action->currentText();
    
    gotoDeclaration(className, "", THCLASS);
}


/**
 * The user selected "Goto implementation" from the delayed class wizard popup.
 */
void ClassView::selectedGotoImplementation()
{
    QString className = classes_action->currentText();
    QString methodName = methods_action->currentText();

    if (methodName.isEmpty())
        gotoDeclaration(className, "", THCLASS);
    else
        gotoImplementation(className, methodName, THPUBLIC_METHOD);
}


/**
 * The user selected "Add method..." from the delayed class wizard popup.
 */
void ClassView::selectedAddMethod()
{
    QString className = classes_action->currentText();
    
    CAddClassMethodDlg dlg(0, "methodDlg");
    if (!dlg.exec())
        return;
    
    CParsedMethod *pm = dlg.asSystemObj();
    pm->setDeclaredInScope(className);

    int atLine = -1;
    CParsedClass *pc = m_store->getClassByName(className);
    
    if (pm->isSignal) {
        for (pc->signalIterator.toFirst(); pc->signalIterator.current(); ++pc->signalIterator) {
            CParsedMethod *meth = pc->signalIterator.current();
            if (meth->exportScope == pm->exportScope && 
                atLine < meth->declarationEndsOnLine)
                atLine = meth->declarationEndsOnLine;
        }
    } else if (pm->isSlot) {
        for (pc->slotIterator.toFirst(); pc->slotIterator.current(); ++pc->slotIterator) {
            CParsedMethod *meth = pc->slotIterator.current();
            if (meth->exportScope == pm->exportScope && 
                atLine < meth->declarationEndsOnLine)
                atLine = meth->declarationEndsOnLine;
        }
    } else {
        for (pc->methodIterator.toFirst(); pc->methodIterator.current(); ++pc->methodIterator) {
            CParsedMethod *meth = pc->methodIterator.current();
            if (meth->exportScope == pm->exportScope && 
                atLine < meth->declarationEndsOnLine)
                atLine = meth->declarationEndsOnLine;
        }
    }

    QString headerCode;
    pm->asHeaderCode(headerCode);
    
    if (atLine == -1) {
        if (pm->isSignal) 
            headerCode.prepend(QString("signals:\n"));
        else if (pm->exportScope == PIE_PUBLIC)
            headerCode.prepend(QString("public:%1\n").arg(pm->isSlot? " slots" :  ""));
        else if (pm->exportScope == PIE_PROTECTED)
            headerCode.prepend(QString("protected:\n").arg(pm->isSlot? " slots" :  ""));
        else if (pm->exportScope == PIE_PRIVATE) 
            headerCode.prepend(QString("private:\n").arg(pm->isSlot? " slots" :  ""));
        else
            kdDebug(9003) << "ClassView::selectedAddMethod: Unknown exportScope "
                          << (int)pm->exportScope << endl;

        atLine = pc->declarationEndsOnLine;
    } else 
        atLine++;

    gotoDeclaration(className, className, THCLASS);
    kdDebug(9003) << "####################" << "Adding at line " << atLine << " " 
                  << headerCode << endl
                  << "####################";

    QString cppCode;
    pm->asCppCode(cppCode);

    gotoSourceFile(pc->definedInFile, atLine);
    kdDebug(9003) << "####################" << "Adding at line " << atLine
                  << " " << cppCode
                  << "####################" << endl;
    
    delete pm;
}


/**
 * The user selected "Add attribute..." from the delayed class wizard popup.
 */
void ClassView::selectedAddAttribute()
{
    QString className = classes_action->currentText();

    CAddClassAttributeDlg dlg(0, "attrDlg");
    if( !dlg.exec() )
      return;

    CParsedAttribute *pa = dlg.asSystemObj();
    pa->setDeclaredInScope(className);

    int atLine = -1;
    CParsedClass *pc = m_store->getClassByName(className);
    
    for (pc->attributeIterator.toFirst(); pc->attributeIterator.current(); ++pc->attributeIterator) {
        CParsedAttribute *attr = pc->attributeIterator.current();
        if (attr->exportScope == pa->exportScope && 
            atLine < attr->declarationEndsOnLine)
            atLine = attr->declarationEndsOnLine;
    }
    
    QString headerCode;
    pa->asHeaderCode(headerCode);
    
    if (atLine == -1) {
        if (pa->exportScope == PIE_PUBLIC)
            headerCode.prepend("public: // Public attributes\n");
        else if (pa->exportScope == PIE_PROTECTED)
            headerCode.prepend("protected: // Protected attributes\n");
        else if (pa->exportScope == PIE_PRIVATE) 
            headerCode.prepend("private: // Private attributes\n");
        else
            kdDebug(9003) << "ClassView::selectedAddAttribute: Unknown exportScope "
                          << (int)pa->exportScope << endl;

        atLine = pc->declarationEndsOnLine;
    } else 
        atLine++;

    gotoDeclaration(className, className, THCLASS);
    kdDebug(9003) << "####################" << "Adding at line " << atLine
                  << " " << headerCode
                  << "####################" << endl;

    delete pa;
}


CParsedClass *ClassView::getClass(const QString &className)
{
    if (className.isEmpty())
        return 0;

    kdDebug(9003) << "ClassView::getClass " << className << endl;
    CParsedClass *pc = m_store->getClassByName(className);
    if (pc && pc->isSubClass)
        classes_action->setCurrentItem(className);
    
    return pc;
}


void ClassView::gotoDeclaration(const QString &className,
                                const QString &declName,
                                THType type)
{
    kdDebug(9003) << "ClassView::gotoDeclaration " << className << "::" << declName << endl;
    
    QString toFile;
    int toLine = -1;
    
    CParsedClass *pc = getClass(className);
    CParsedStruct *ps = 0;
    CParsedAttribute *pa = 0;
    
    switch(type) {
    case THCLASS:
        toFile = pc->declaredInFile;
        toLine = pc->declaredOnLine;
        break;
    case THSTRUCT:
        if (pc)
            pc->getStructByName(declName);
        else
            ps = m_store->globalContainer.getStructByName(declName);
        toFile = ps->declaredInFile;
        toLine = ps->declaredOnLine;
        break;
    case THPUBLIC_ATTR:
    case THPROTECTED_ATTR:
    case THPRIVATE_ATTR:
        if (pc)
            pa = pc->getAttributeByName(declName);
        else {
            ps = m_store->globalContainer.getStructByName(className);
            if (ps)
                pa = ps->getAttributeByName(declName);
        }
        break;
    case THPUBLIC_METHOD:
    case THPROTECTED_METHOD:
    case THPRIVATE_METHOD:
        pa = pc->getMethodByNameAndArg(declName);
        // If at first we don't succeed...
        if (!pa)
            pa = pc->getSlotByNameAndArg(declName);      
        break;
    case THPUBLIC_SLOT:
    case THPROTECTED_SLOT:
    case THPRIVATE_SLOT:
        pa = pc->getSlotByNameAndArg(declName);
      break;
    case THSIGNAL:
        pa = pc->getSignalByNameAndArg(declName);
      break;
    case THGLOBAL_FUNCTION:
        pa = m_store->globalContainer.getMethodByNameAndArg(declName);
      break;
    case THGLOBAL_VARIABLE:
        pa = m_store->globalContainer.getAttributeByName(declName);
        break;
    default:
        kdDebug(9003) << "Unknown type " << (int)type << " in CVGotoDeclaration." << endl;
        break;
    }
    
    // Fetch the line and file from the attribute if the value is set.
    if (pa) {
        toFile = pa->declaredInFile;
        toLine = pa->declaredOnLine;
    }
    
    if (toLine != -1) {
        kdDebug(9003) << "Classview switching to file " << toFile << "@ line " << toLine << endl;
        emit gotoSourceFile(toFile, toLine);
    }
}


void ClassView::gotoImplementation(const QString &className,
                                   const QString &declName,
                                   THType type)
{
    kdDebug(9003) << "ClassView::gotoImplementation " << className << "::" << declName << endl;
    CParsedClass *pc = getClass(className);
    CParsedMethod *pm = 0;
    
    switch(type) {
    case THPUBLIC_SLOT:
    case THPROTECTED_SLOT:
    case THPRIVATE_SLOT:
        if (pc)
            pm = pc->getSlotByNameAndArg(declName);
        break;
    case THPUBLIC_METHOD:
    case THPROTECTED_METHOD:
    case THPRIVATE_METHOD:
        if (pc) {
            pm = pc->getMethodByNameAndArg(declName);
            // If at first we don't succeed...
            if (!pm)
                pm = pc->getSlotByNameAndArg(declName); 
        }
        break;
    case THGLOBAL_FUNCTION:
        pm = m_store->globalContainer.getMethodByNameAndArg(declName);
        break;
    default:
        kdDebug(9003) << "Unknown type " << (int)type << "in CVGotoDefinition." << endl;
    }
    
    if (pm)
        emit gotoSourceFile(pm->definedInFile, pm->definedOnLine);
}
