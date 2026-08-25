# ---------------------------------------------------------------------------
# Author:      Dave Register DEB config from: antonm - Anton Martchukov
# <anton@martchukov.com>
# ***************************************************************************
# - Copyright (C) 2010 by David S. Register                               *
# - This program is free software; you can redistribute it and/or modify  *
# - it under the terms of the GNU General Public License as published by  *
# - the Free Software Foundation; either version 2 of the License, or     *
# - (at your option) any later version.                                   *
# - *
# - This program is distributed in the hope that it will be useful,       *
# - but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# - MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# - GNU General Public License for more details.                          *
# - *
# - You should have received a copy of the GNU General Public License     *
# - along with this program; if not, write to the                         *
# - Free Software Foundation, Inc.,                                       *
# - 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
# ***************************************************************************

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
MESSAGE (STATUS "*** Staging to build PlugIns ***")
# These variables are inherited by all plugins
set(PARENT opencpn)
set(PREFIX_DATA share)
# Keep the package destination relative.  PREFIX_LIB expands to an absolute
# path in distro builds; using it here installs correctly through DESTDIR but
# makes CPack treat bundled plug-in libraries as external absolute files and
# omit them from the archive.  CMAKE_INSTALL_LIBDIR preserves lib/lib64
# selection while allowing both staged installs and CPack packages to contain
# the plug-ins.
set(PREFIX_PLUGINS ${CMAKE_INSTALL_LIBDIR}/${PARENT})

set(STANDALONE "BUNDLED")

file(GLOB PLUGINS *_pi)

foreach (PLUGIN ${PLUGINS})
  add_subdirectory(${PLUGIN})
  message(STATUS "*** Added plugin: ${PLUGIN}")
endforeach ()
