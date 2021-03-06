/*
 * PDFedit - free program for PDF document manipulation.
 * Copyright (C) 2006-2009  PDFedit team: Michal Hocko,
 *                                        Jozef Misutka,
 *                                        Martin Petricek
 *                   Former team members: Miroslav Jahoda
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in doc/LICENSE.GPL); if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA  02111-1307  USA
 *
 * Project is hosted on http://sourceforge.net/projects/pdfedit
 */
#ifndef __QSWRAPPER_H__
#define __QSWRAPPER_H__

#include <qobject.h>
#include <qswrapperfactory.h>

namespace gui {

/**
 Wrapper factory that will return the object itself, thus handling its deallocation to QSA
 \brief QObject Wrapper factory
 */
class QSWrapper : public QSWrapperFactory {
public:
 QSWrapper();
 virtual ~QSWrapper();
 virtual QObject* create(const QString &className, void *ptr);
};

} // namespace gui

#endif
