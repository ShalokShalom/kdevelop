/***************************************************************************
                          GccCompilerOptionsWidget.cpp  -  description
                             -------------------
    begin                : Mon Feb 5 2001
    copyright            : (C) 2001 by Omid Givi & Bernd Gehrmann
    email                : omid@givi.nl
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "GccCompilerOptionsWidget.h"
#include "GccCompilerOptionsWidgetBase.h"
#include <qlineedit.h>
#include <qobject.h>
#include "kdebug.h"
#include <qwidget.h>
#include <qvbuttongroup.h>
#include <qvbox.h>
#include <qlayout.h>
#include <klocale.h>
#include <qapplication.h>
#include <qlabel.h>
#include <qvbuttongroup.h>
#include <qvbox.h>
#include <kdialog.h>

GccCompilerOptionsWidget::GccCompilerOptionsWidget(QWidget *parent, const char *name, QWidget* pdlg)
	 : GccCompilerOptionsWidgetBase(parent,name) {
	compilerFlagsPreview->setReadOnly(true);
	connect(pdlg, SIGNAL(WidgetStarted(KDevCompiler*)), this, SLOT(slotWidgetStarted(KDevCompiler*)));
	connect(pdlg, SIGNAL(ButtonApplyClicked(KDevCompiler*)), this, SLOT(slotButtonApplyClicked(KDevCompiler*)));
}
GccCompilerOptionsWidget::~GccCompilerOptionsWidget(){
}

void GccCompilerOptionsWidget::slotFlagsToolButtonClicked(){
 	GccOptionsDlg *dlg = new GccOptionsDlg(this, "Gcc Project Options Dialog");
 	compilerFlagsPreview->setText(dlg->setFlags(compilerFlagsPreview->text()));
 	dlg->exec();
 	compilerFlagsPreview->setText(dlg->flags() + " " + compilerFlagsPreview->text());
 	delete dlg;
}

// reads the compiler flags
void GccCompilerOptionsWidget::slotWidgetStarted(KDevCompiler *kdc){
	if (*(kdc->name()) == "gcc"){
  	compilerFlagsPreview->setText(*(kdc->flags()));
	}
}

// writess the compiler flags
void GccCompilerOptionsWidget::slotButtonApplyClicked(KDevCompiler *kdc){
	if (*(kdc->name()) == "gcc"){
		compilerFlagsPreview->setText(compilerFlagsPreview->text() + " " + gccCompilerFlags->text());
		gccCompilerFlags->setText("");
  	kdc->setFlags(compilerFlagsPreview->text());
  }
}

void GccCompilerOptionsWidget::slotClearAllClicked(){
  compilerFlagsPreview->setText("");
}

GeneralTabGcc::GeneralTabGcc(QWidget *parent, const char *name)
    : QWidget(parent, name), controller(new FlagCheckBoxController)
{
    QBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    layout->setAutoAdd(true);

    new FlagCheckBox(this, controller,
                     "-fsyntax-only", i18n("Only check the code for syntax errors. Don't produce object code"));
    new FlagCheckBox(this, controller,
                     "-fg",           i18n("Generate extra code to write profile information for gprof."));
    new FlagCheckBox(this, controller,
                     "-save-temps",   i18n("Do not delete intermediate output like assembler files."));

    layout->addSpacing(10);
    QApplication::sendPostedEvents(this, QEvent::ChildInserted);
    layout->addStretch();
}


GeneralTabGcc::~GeneralTabGcc()
{
    delete controller;
}


void GeneralTabGcc::readFlags(QStringList *list)
{
    controller->readFlags(list);
}


void GeneralTabGcc::writeFlags(QStringList *list)
{
    controller->writeFlags(list);
}


CodeGenTabGcc::CodeGenTabGcc(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    QBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    layout->setAutoAdd(true);

    genBox = new FlagListBox(this);

    new FlagListItem(genBox,
                     "-fexceptions",        i18n("Enable exception handling."),
                     "-fno-exception");
    // The following two are somehow mutually exclusive, but the default is
    // platform-dependent, so if we would leave out one of them, we wouldn't
    // know how to set the remaining one.
    new FlagListItem(genBox,
                     "-fpcc-struct-return", i18n("<qt>Return certain struct and union values in memory rather than in registers.</qt>"));
    new FlagListItem(genBox,
                     "-freg-struct-return", i18n("<qt>Return certain struct and union values in registers when possible.</qt>"));
    new FlagListItem(genBox,
                     "-short-enums",        i18n("<qt>For an enum, choose the smallest possible integer type.</qt>"));
    new FlagListItem(genBox,
                     "-short-double",       i18n("<qt>Make <i>double</i> the same as <i>float</i>.</qt>"));

    layout->addSpacing(10);
    QApplication::sendPostedEvents(this, QEvent::ChildInserted);
    layout->addStretch();
}


CodeGenTabGcc::~CodeGenTabGcc()
{
}


void CodeGenTabGcc::readFlags(QStringList *list)
{
    genBox->readFlags(list);
}


void CodeGenTabGcc::writeFlags(QStringList *list)
{
    genBox->writeFlags(list);
}


OptimizationTabGcc::OptimizationTabGcc(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    QBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    layout->setAutoAdd(true);

    QVButtonGroup *group = new QVButtonGroup(i18n("Optimization Level"), this);
    Odefault = new QRadioButton(i18n("Default"), group);
    Odefault->setChecked(true);
    O0 = new QRadioButton(i18n("No Optimization"), group);
    O1 = new QRadioButton(i18n("Level 1"), group);
    O2 = new QRadioButton(i18n("Level 2"), group);

    optBox = new FlagListBox(this);

    new FlagListItem(optBox,
                     "-ffloat-store",       i18n("<qt>Do not store floating point variables in registers.</qt>"),
                     "-fno-float-store");
    new FlagListItem(optBox,
                     "-fno-defer-pop",      i18n("<qt>Pop the arguments to each function call directly "
                                                 "after the function returns.</qt>"),
                     "-fdefer-pop");
    new FlagListItem(optBox,
                     "-fforce-mem",         i18n("<qt>Force memory operands to be copied into registers before "
                                                 "doing arithmetic on them.</qt>"),
                     "-fno-force-mem");
    new FlagListItem(optBox,
                     "-fforce-addr",        i18n("<qt>Force memory address constants to be copied into registers before "
                                                 "doing arithmetic on them.</qt>"),
                     "-fno-force-addr");
    new FlagListItem(optBox,
                     "-omit-frame-pointer", i18n("<qt>Don't keep the frame pointer in a register for functions that "
                                                 "don't need one.</qt>"),
                     "-fno-omit-frame-pointer");
    new FlagListItem(optBox,
                     "-no-inline",          i18n("<qt>Ignore the <i>inline</i> keyword.</qt>"),
                     "-finline");

    QApplication::sendPostedEvents(this, QEvent::ChildInserted);
    layout->addStretch();
}


OptimizationTabGcc::~OptimizationTabGcc()
{}


void OptimizationTabGcc::readFlags(QStringList *list)
{
    optBox->readFlags(list);

    QStringList::Iterator sli;
    sli = list->find("-O0");
    if (sli != list->end()) {
        O0->setChecked(true);
        list->remove(sli);
    }
    sli = list->find("-O1");
    if (sli != list->end()) {
        O1->setChecked(true);
        list->remove(sli);
    }
    sli = list->find("-O2");
    if (sli != list->end()) {
        O2->setChecked(true);
        list->remove(sli);
    }
}


void OptimizationTabGcc::writeFlags(QStringList *list)
{
    optBox->writeFlags(list);

    if (O0->isChecked())
        (*list) << "-O0";
    else if (O1->isChecked())
        (*list) << "-O1";
    else if (O2->isChecked())
        (*list) << "-O2";
}


Warnings1TabGcc::Warnings1TabGcc(QWidget *parent, const char *name)
    : QWidget(parent, name), controller(new FlagCheckBoxController)
{
    QBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    layout->setAutoAdd(true);

    new FlagCheckBox(this, controller,
                     "-w",               i18n("Inhibit all warnings."));
    new FlagCheckBox(this, controller,
                     "-Wno-import",      i18n("Inhibit warnings about the use of #import."));
    new FlagCheckBox(this, controller,
                     "-Werror", 	 i18n("Make all warnings into errors."));
    new FlagCheckBox(this, controller,
                     "-pedantic",        i18n("Issue all warnings demanded by strict ANSI C or ISO C++."));
    new FlagCheckBox(this, controller,
                     "-pedantic-errors", i18n("Like -pedantic, but errors are produced instead of warnings."));
    new FlagCheckBox(this, controller,
                     "-Wall",            i18n("All below warnings combined:"));

    wallBox = new FlagListBox(this);

    new FlagListItem(wallBox,
                     "-Wchar-subscripts",    i18n("<qt>Warn if an array subscript has type <i>char</i>.</qt>"));
    new FlagListItem(wallBox,
                     "-Wcomment",            i18n("<qt>Warn when a comment-start sequence /* appears inside a comment.</qt>"));
    new FlagListItem(wallBox,
                     "-Wformat",             i18n("<qt>Check calls to <i>printf()</i>, <i>scanf()</i> etc.</qt>"));
    new FlagListItem(wallBox,
                     "-Wimplicit-int",       i18n("<qt>Warn when a declaration does not specify a type.</qt>"));
    new FlagListItem(wallBox,
                     "-Wimplicit-funtion-declaration",
                                             i18n("<qt>Issue a warning when a non-declared function is used.</qt>"));
    new FlagListItem(wallBox,
                     "-Werror-implicit-function-declaration",
                                             i18n("<qt>Issue an error when a non-declared function is used.</qt>"));
    new FlagListItem(wallBox,
                     "-Wmain",               i18n("<qt>Warn if the type of <i>main()</i> is suspicious.</qt>"));
    new FlagListItem(wallBox,
                     "-Wmultichar",          i18n("<qt>Warn when multicharacter constants are encountered.</qt>"));
    new FlagListItem(wallBox,
                     "-Wparentheses",        i18n("<qt>Warn when parentheses are omitted in certain context.</qt>"));
    new FlagListItem(wallBox,
                     "-Wreturn-type",        i18n("<qt>Warn when a function without explicit return type is defined.</qt>"));
    new FlagListItem(wallBox,
                     "-Wtrigraphs",          i18n("<qt>Warn when trigraphs are encountered.</qt>"));
    new FlagListItem(wallBox,
                     "-Wunused",             i18n("<qt>Warn when a variable is declared but not used.</qt>"));
    new FlagListItem(wallBox,
                     "-Wuninitialized",      i18n("<qt>Warn when a variable is used without being initialized before.</qt>"));
    new FlagListItem(wallBox,
                     "-Wunknown-pragmas",    i18n("<qt>Warn when an unknown #pragma statement is encountered.</qt>"));
}


Warnings1TabGcc::~Warnings1TabGcc()
{
    delete controller;
}


void Warnings1TabGcc::readFlags(QStringList *list)
{
    controller->readFlags(list);
    wallBox->readFlags(list);
}


void Warnings1TabGcc::writeFlags(QStringList *list)
{
    controller->writeFlags(list);
    wallBox->readFlags(list);
}


Warnings2TabGcc::Warnings2TabGcc(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    QBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    layout->setAutoAdd(true);

    wrestBox = new FlagListBox(this);

    new FlagListItem(wrestBox,
                     "-W",                    i18n("<qt>Set options not included in -Wall which are very specific.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wtraditional",         i18n("<qt>Warn about certain constructs that behave differently\n"
                                                   "in traditional and ANSI C.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wundef",               i18n("<qt>Warn if an undefined identifier is evaluated in an `#if' directive</qt>"));
    new FlagListItem(wrestBox,
                     "-Wshadow",              i18n("<qt>Warn whenever a local variable shadows another local variable.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wpointer-arith",       i18n("<qt>Warn about anything that depends on the <i>sizeof</i> a\n"
                                                   "function type or of <i>void</i>.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wbad-function-cast",   i18n("<qt>Warn whenever a function call is cast to a non-matching type.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wcast-qual",           i18n("<qt>Warn whenever a pointer is cast so as to remove a type\n"
                                                   "qualifier from the target type.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wcast-align",          i18n("<qt>Warn whenever a pointer is cast such that the required\n"
                                                   "alignment of the target is increased.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wwrite-strings",       i18n("<qt>Warn when the address of a string constant is cast\n"
                                                   "into a non-const <i>char *</i> pointer</qt>"));
    new FlagListItem(wrestBox,
                     "-Wconversion",          i18n("<qt>Warn if a prototype causes a type conversion that is different\n"
                                                   "from what would happen to the same argument in the absence\n"
                                                   "of a prototype.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wsign-compare",        i18n("<qt>Warn when a comparison between signed and unsigned values\n"
                                                   "could produce an incorrect result when the signed value\n"
                                                   "is converted to unsigned.</qt>"));
    new FlagListItem(wrestBox,
                     "-Waggregate-return",    i18n("<qt>Warn if any functions that return structures or unions are\n"
                                                   "defined or called.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wstrict-prototypes",   i18n("<qt>Warn if a function is declared or defined without specifying\n"
                                                   "the argument types.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wmissing-prototypes",  i18n("<qt>Warn if a global function is defined without a previous prototype declaration.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wmissing-declarations",i18n("<qt>Warn if a global function is defined without a previous declaration.</qt>"));
    new FlagListItem(wrestBox,
                     "-Wredundant-decls",     i18n("<qt>Warn if anything is declared more than once in the same scope</qt>"));
    new FlagListItem(wrestBox,
                     "-Wnested-externs",      i18n("<qt>Warn if an <i>extern</i> declaration is encountered within a function.</qt>"));
    new FlagListItem(wrestBox,
                     "-Winline",              i18n("<qt>Warn if an <i>inline</qt> function can not be inlined</qt>"));
    new FlagListItem(wrestBox,
                     "-Wold-style-cast",      i18n("<qt>Warn if an old-style (C-style) cast is used within a program</qt>"));
    new FlagListItem(wrestBox,
                     "-Wlong-long",           i18n("<qt>Warn if the <i>long long</i> type is used.</qt>"));

}


Warnings2TabGcc::~Warnings2TabGcc()
{}


void Warnings2TabGcc::readFlags(QStringList *list)
{
    wrestBox->readFlags(list);
}


void Warnings2TabGcc::writeFlags(QStringList *list)
{
    wrestBox->writeFlags(list);
}


GccOptionsDlg::GccOptionsDlg(QWidget *parent, const char *name)
    : KDialogBase(Tabbed, i18n("GNU C Compiler Options"), Ok|Cancel, Ok,parent, name, true)
{
    QVBox *vbox;

    vbox = addVBoxPage(i18n("General"));
    general = new GeneralTabGcc(vbox, "general tab");

    vbox = addVBoxPage(i18n("Code Generation"));
    codegen = new CodeGenTabGcc(vbox, "codegen tab");

    vbox = addVBoxPage(i18n("Optimization"));
    optimization = new OptimizationTabGcc(vbox, "optimization tab");

    vbox = addVBoxPage(i18n("Warnings I"));
    warnings1 = new Warnings1TabGcc(vbox, "warnings1 tab");

    vbox = addVBoxPage(i18n("Warnings II"));
    warnings2 = new Warnings2TabGcc(vbox, "warnings2 tab");
}


GccOptionsDlg::~GccOptionsDlg()
{
}

QString GccOptionsDlg::flags() const
{
    QStringList flaglist;

    optimization->writeFlags(&flaglist);
    warnings1->writeFlags(&flaglist);
    warnings2->writeFlags(&flaglist);
    codegen->writeFlags(&flaglist);
    general->writeFlags(&flaglist);

    QString flags;
    QStringList::ConstIterator li;
    for (li = flaglist.begin(); li != flaglist.end(); ++li) {
        flags += (*li);
        flags += " ";
    }

    flags.truncate(flags.length()-1);
    return flags;
}

QString GccOptionsDlg::setFlags(const QString &flags)
{
    QStringList flaglist = QStringList::split(" ", flags);

    optimization->readFlags(&flaglist);
    warnings1->readFlags(&flaglist);
    warnings2->readFlags(&flaglist);
    codegen->readFlags(&flaglist);
    general->readFlags(&flaglist);

    // lets see what we have over and return it
    QString flagsRest;
    QStringList::ConstIterator li;
    for (li = flaglist.begin(); li != flaglist.end(); ++li) {
        flagsRest += (*li);
        flagsRest += " ";
    }

    flagsRest.truncate(flagsRest.length()-1);
    return flagsRest;
}


#include "GccCompilerOptionsWidget.moc"
