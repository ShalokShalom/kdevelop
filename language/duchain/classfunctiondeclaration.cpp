/* This  is part of KDevelop
    Copyright 2002-2005 Roberto Raggi <roberto@kdevelop.org>
    Copyright 2006 Adam Treat <treat@kde.org>
    Copyright 2006 Hamish Rodda <rodda@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "classfunctiondeclaration.h"

#include "ducontext.h"
#include "typesystem.h"

namespace KDevelop
{
Identifier conversionIdentifier("operator{...cast...}");

class ClassFunctionDeclarationPrivate
{
public:
  ClassFunctionDeclaration::QtFunctionType m_functionType;
};

ClassFunctionDeclaration::ClassFunctionDeclaration(const ClassFunctionDeclaration& rhs) : ClassMemberDeclaration(rhs), AbstractFunctionDeclaration(rhs), d(new ClassFunctionDeclarationPrivate) {
  d->m_functionType = rhs.d->m_functionType;
}

void ClassFunctionDeclaration::setAbstractType(AbstractType::Ptr type) {
  Q_ASSERT( !type || dynamic_cast<FunctionType*>(type.data()) );
  ClassMemberDeclaration::setAbstractType(type);
}

ClassFunctionDeclaration::ClassFunctionDeclaration(const HashedString& url, KTextEditor::Range * range, DUContext* context)
  : ClassMemberDeclaration(url, range, context), AbstractFunctionDeclaration()
  , d(new ClassFunctionDeclarationPrivate)
{
  d->m_functionType = Normal;
}

Declaration* ClassFunctionDeclaration::clone() const {
  return new ClassFunctionDeclaration(*this);
}

ClassFunctionDeclaration::~ClassFunctionDeclaration()
{
  delete d;
}

QString ClassFunctionDeclaration::toString() const {
  if( !abstractType() )
    return ClassMemberDeclaration::toString();

  KSharedPtr<FunctionType> function = type<FunctionType>();
  Q_ASSERT(function);
  return QString("member-function %1 %2 %3").arg(function->partToString( FunctionType::SignatureReturn )).arg(identifier().toString()).arg(function->partToString( FunctionType::SignatureArguments ));
}


/*bool ClassFunctionDeclaration::isSimilar(KDevelop::CodeItem *other, bool strict ) const
{
  if (!CppClassMemberType::isSimilar(other,strict))
    return false;

  FunctionModelItem func = dynamic_cast<ClassFunctionDeclaration*>(other);

  if (isConstant() != func->isConstant())
    return false;

  if (arguments().count() != func->arguments().count())
    return false;

  for (int i=0; i<arguments().count(); ++i)
    {
      ArgumentModelItem arg1 = arguments().at(i);
      ArgumentModelItem arg2 = arguments().at(i);

      if (arg1->type() != arg2->type())
        return false;
    }

  return true;
}*/

ClassFunctionDeclaration::QtFunctionType ClassFunctionDeclaration::functionType() const
{
  return d->m_functionType;
}

void ClassFunctionDeclaration::setFunctionType(QtFunctionType functionType)
{
  d->m_functionType = functionType;
}

bool ClassFunctionDeclaration::isConversionFunction() const {
  return identifier() == conversionIdentifier;
}

bool ClassFunctionDeclaration::isConstructor() const
{
  if (context() && context()->type() == DUContext::Class && context()->localScopeIdentifier().top().nameEquals(identifier()))
    return true;
  return false;
}

bool ClassFunctionDeclaration::isDestructor() const
{
  QString id = identifier().toString();
  return context() && context()->type() == DUContext::Class && id.startsWith('~') && id.mid(1) == context()->localScopeIdentifier().top().toString();
}
}
// kate: space-indent on; indent-width 2; tab-width 4; replace-tabs on; auto-insert-doxygen on
