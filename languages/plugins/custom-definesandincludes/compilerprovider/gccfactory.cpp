/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2014  <copyright holder> <mail_subscriber@mail.ru>
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

#include "gccfactory.h"

#include "gcclikecompiler.h"

QString GccFactory::name() const
{
    return "GCC";
}

Compiler GccFactory::createCompiler(const QString& name, const QString& path, bool editable )
{
    static Compiler compiler = { CompilerPointer(new GccLikeCompiler()), "", "", true};
    compiler.name = name;
    compiler.path = path;
    compiler.editable = editable;

    return compiler;
}