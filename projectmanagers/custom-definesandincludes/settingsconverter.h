/*
 * This file is part of KDevelop
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef SETTINGSCONVERTER_H
#define SETTINGSCONVERTER_H

#include <QList>

#include <language/interfaces/idefinesandincludesmanager.h>

class KConfig;
class SettingsManager;

using KDevelop::ConfigEntry;

struct SettingsConverter
{
public:
    QList <ConfigEntry> readSettings( KConfig* cfg ) const;
    const SettingsManager* manager;
};

#endif // SETTINGSCONVERTER_H
