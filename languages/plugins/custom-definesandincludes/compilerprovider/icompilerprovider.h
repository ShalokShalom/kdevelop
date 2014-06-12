/*
 * This file is part of KDevelop
 *
 * Copyright 2014 Sergey Kalinichev <kalinichev.so.0@gmail.com>
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

#ifndef ICOMPILERPROVIDER_H
#define ICOMPILERPROVIDER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "icompiler.h"
#include "icompilerfactory.h"

namespace KDevelop
{
class IProject;
}

class ICompilerProvider
{
public:
    /// Select the @p compiler that provides standard includes/defines for the @p project
    /// @return true on success, false otherwise, e.g. if the compiler isn't available.
    virtual bool setCompiler( KDevelop::IProject* project, Compiler compiler ) = 0;

    /// @return current compiler for the @P project
    virtual Compiler currentCompiler( KDevelop::IProject* project ) const = 0;

    /// @return list of all available compilers
    virtual QVector<Compiler> compilers() const = 0;

    /// @return All available factories
    virtual QVector<CompilerFactoryPointer> compilerFactories() const = 0;

    /// Adds compiler to the list of available compilers
    /// @return true on success
    virtual bool registerCompiler(const Compiler& compiler) = 0;

    /// Removes compiler from the list of available compilers
    virtual void unregisterCompiler(const Compiler& compiler) = 0;

    virtual ~ICompilerProvider() = default;
};

Q_DECLARE_INTERFACE( ICompilerProvider, "org.kdevelop.ICompilerProvider" )

#endif // ICOMPILERPROVIDER_H
