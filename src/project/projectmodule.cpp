/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "projectmodule.h"

#include <QQmlEngine>

#include "modularity/ioc.h"
#include "internal/projectcreator.h"
#include "internal/notationreadersregister.h"
#include "internal/notationwritersregister.h"
#include "internal/projectautosaver.h"
#include "internal/projectfilescontroller.h"
#include "internal/projectuiactions.h"
#include "internal/projectconfiguration.h"
#include "internal/exportprojectscenario.h"
#include "internal/recentprojectsprovider.h"
#include "internal/mscmetareader.h"

#include "view/exportdialogmodel.h"
#include "view/recentprojectsmodel.h"

#ifdef Q_OS_MAC
#include "internal/platform/macos/macosrecentfilescontroller.h"
#elif defined (Q_OS_WIN)
#include "internal/platform/windows/windowsrecentfilescontroller.h"
#else
#include "internal/platform/stub/stubrecentfilescontroller.h"
#endif

#include "ui/iuiactionsregister.h"

using namespace mu::project;
using namespace mu::modularity;

static std::shared_ptr<ProjectConfiguration> s_configuration = std::make_shared<ProjectConfiguration>();
static std::shared_ptr<ProjectFilesController> s_fileController = std::make_shared<ProjectFilesController>();
static std::shared_ptr<RecentProjectsProvider> s_recentProjectsProvider = std::make_shared<RecentProjectsProvider>();

std::string ProjectModule::moduleName() const
{
    return "project";
}

void ProjectModule::registerExports()
{
    ioc()->registerExport<IProjectConfiguration>(moduleName(), s_configuration);
    ioc()->registerExport<IProjectCreator>(moduleName(), new ProjectCreator());
    ioc()->registerExport<INotationReadersRegister>(moduleName(), new NotationReadersRegister());
    ioc()->registerExport<INotationWritersRegister>(moduleName(), new NotationWritersRegister());
    ioc()->registerExport<IProjectFilesController>(moduleName(), s_fileController);
    ioc()->registerExport<IExportProjectScenario>(moduleName(), new ExportProjectScenario());
    ioc()->registerExport<IRecentProjectsProvider>(moduleName(), s_recentProjectsProvider);
    ioc()->registerExport<IMscMetaReader>(moduleName(), new MscMetaReader());

#ifdef Q_OS_MAC
    ioc()->registerExport<IPlatformRecentFilesController>(moduleName(), new MacOSRecentFilesController());
#elif defined (Q_OS_WIN)
    ioc()->registerExport<IPlatformRecentFilesController>(moduleName(), new WindowsRecentFilesController());
#else
    ioc()->registerExport<IPlatformRecentFilesController>(moduleName(), new StubRecentFilesController());
#endif
}

void ProjectModule::resolveImports()
{
    auto ar = ioc()->resolve<ui::IUiActionsRegister>(moduleName());
    if (ar) {
        ar->reg(std::make_shared<UserScoresUiActions>(s_fileController));
    }
}

void ProjectModule::registerUiTypes()
{
    qmlRegisterType<ExportDialogModel>("MuseScore.UserScores", 1, 0, "ExportDialogModel");
    qmlRegisterType<RecentProjectsModel>("MuseScore.UserScores", 1, 0, "RecentScoresModel");
}

void ProjectModule::onInit(const framework::IApplication::RunMode& mode)
{
    if (framework::IApplication::RunMode::Editor != mode) {
        return;
    }

    s_configuration->init();
    s_fileController->init();
    s_recentProjectsProvider->init();

    static ProjectAutoSaver autoSaver;
    autoSaver.init();
}
