# SPDX-License-Identifier: GPL-3.0-only
# MuseScore-CLA-applies
#
# MuseScore
# Music Composition & Notation
#
# Copyright (C) 2021 MuseScore BVBA and others
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

set(MODULE vst)

add_subdirectory("sdk")

include(GetPlatformInfo)

set(MODULE_QRC vst.qrc)

set(MODULE_QML_IMPORT ${CMAKE_CURRENT_LIST_DIR}/qml )

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/ivstmodulesrepository.h
    ${CMAKE_CURRENT_LIST_DIR}/ivstconfiguration.h
    ${CMAKE_CURRENT_LIST_DIR}/ivstpluginsregister.h
    ${CMAKE_CURRENT_LIST_DIR}/vsterrors.h
    ${CMAKE_CURRENT_LIST_DIR}/vsttypes.h
    ${CMAKE_CURRENT_LIST_DIR}/vstmodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vstmodule.h

    ${CMAKE_CURRENT_LIST_DIR}/internal/vstmodulesrepository.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstmodulesrepository.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstpluginsregister.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstpluginsregister.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstconfiguration.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstconfiguration.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstplugin.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstplugin.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstaudioclient.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/vstaudioclient.h

    ${CMAKE_CURRENT_LIST_DIR}/internal/synth/vstsynthesiser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/synth/vstsynthesiser.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/synth/vstiresolver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/synth/vstiresolver.h

    ${CMAKE_CURRENT_LIST_DIR}/internal/fx/vstfxprocessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/fx/vstfxprocessor.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/fx/vstfxresolver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/fx/vstfxresolver.h

    ${CMAKE_CURRENT_LIST_DIR}/devtools/vstpluginlistmodelexample.cpp
    ${CMAKE_CURRENT_LIST_DIR}/devtools/vstpluginlistmodelexample.h

    ${CMAKE_CURRENT_LIST_DIR}/view/vstieditorview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/vstieditorview.h
    ${CMAKE_CURRENT_LIST_DIR}/view/vstfxeditorview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/vstfxeditorview.h
    )

set(MODULE_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}
    ${VST3_SDK_PATH}
    )

set(MODULE_LINK
    vst_sdk_3
    )

if (OS_IS_LIN)
    set(MODULE_LINK
        ${MODULE_LINK}
        stdc++fs
        )
endif()

include(${PROJECT_SOURCE_DIR}/build/module.cmake)

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/VST3)
