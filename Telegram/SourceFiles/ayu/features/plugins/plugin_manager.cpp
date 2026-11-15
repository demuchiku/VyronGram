#include "ayu/features/plugins/plugin_manager.h"

#include "ayu/libs/json.hpp"
#include "core/application.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QApplication>

using json = nlohmann::json;

namespace Vyron {
namespace {

typedef bool (*PluginInitFn)(const char *version);
typedef void (*PluginCleanupFn)();

} // namespace

PluginManager &PluginManager::Instance() {
	static PluginManager instance;
	return instance;
}

QString PluginManager::pluginsPath() const {
	return cWorkingDir() + u"tdata/plugins/"_q;
}

void PluginManager::init() {
	const auto dir = QDir(pluginsPath());
	if (!dir.exists()) {
		dir.mkpath(u"."_q);
	}
	ensureDefaultPluginExists();
	loadConfig();
	scan();
	loadEnabledPlugins();
}

void PluginManager::ensureDefaultPluginExists() {
	const auto exampleDir = QDir(pluginsPath() + u"example_theme/"_q);
	if (!exampleDir.exists()) {
		exampleDir.mkpath(u"."_q);
		auto jsonFile = QFile(exampleDir.filePath(u"plugin.json"_q));
		if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QTextStream stream(&jsonFile);
			stream << "{\n"
				   << "  \"id\": \"example_theme\",\n"
				   << "  \"name\": \"Example Amber Theme\",\n"
				   << "  \"version\": \"1.0.0\",\n"
				   << "  \"author\": \"VyronGram\",\n"
				   << "  \"description\": \"Demonstrates external plugin custom stylesheet support.\",\n"
				   << "  \"style\": \"style.qss\",\n"
				   << "  \"enabled\": false\n"
				   << "}\n";
			jsonFile.close();
		}
		auto qssFile = QFile(exampleDir.filePath(u"style.qss"_q));
		if (qssFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QTextStream stream(&qssFile);
			stream << "/* VyronGram Plugin Stylesheet */\n"
				   << "/* QWidget { font-family: 'Segoe UI'; } */\n";
			qssFile.close();
		}
	}
}

void PluginManager::loadConfig() {
	_enabledIds.clear();
	auto file = QFile(cWorkingDir() + u"tdata/vyron_plugins.json"_q);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	try {
		const auto data = json::parse(file.readAll().toStdString());
		if (data.contains("enabled") && data["enabled"].is_array()) {
			for (const auto &item : data["enabled"]) {
				if (item.is_string()) {
					_enabledIds.append(QString::fromStdString(item.get<std::string>()));
				}
			}
		}
	} catch (...) {
	}
	file.close();
}

void PluginManager::saveConfig() {
	auto file = QFile(cWorkingDir() + u"tdata/vyron_plugins.json"_q);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return;
	}
	try {
		json data;
		auto array = json::array();
		for (const auto &id : _enabledIds) {
			array.push_back(id.toStdString());
		}
		data["enabled"] = array;
		const auto dumped = data.dump(2);
		file.write(dumped.data(), dumped.size());
	} catch (...) {
	}
	file.close();
}

void PluginManager::scan() {
	_plugins.clear();
	const auto root = QDir(pluginsPath());
	if (!root.exists()) {
		return;
	}

	const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const auto &entry : entries) {
		const auto dir = QDir(entry.absoluteFilePath());
		const auto manifestPath = dir.filePath(u"plugin.json"_q);
		if (!QFile::exists(manifestPath)) {
			continue;
		}

		auto manifestFile = QFile(manifestPath);
		if (!manifestFile.open(QIODevice::ReadOnly)) {
			continue;
		}

		try {
			const auto parsed = json::parse(manifestFile.readAll().toStdString());
			PluginInfo info;
			info.dirPath = entry.absoluteFilePath();
			info.id = QString::fromStdString(parsed.value("id", entry.fileName().toStdString()));
			info.name = QString::fromStdString(parsed.value("name", entry.fileName().toStdString()));
			info.version = QString::fromStdString(parsed.value("version", "1.0.0"));
			info.author = QString::fromStdString(parsed.value("author", "Unknown"));
			info.description = QString::fromStdString(parsed.value("description", ""));
			info.styleFile = QString::fromStdString(parsed.value("style", ""));
			info.libraryFile = QString::fromStdString(parsed.value("library", ""));
			info.enabled = _enabledIds.contains(info.id);
			_plugins.push_back(std::move(info));
		} catch (...) {
		}
		manifestFile.close();
	}
}

void PluginManager::unloadAll() {
	for (auto &plugin : _plugins) {
		if (plugin.library && plugin.library->isLoaded()) {
			const auto cleanup = reinterpret_cast<PluginCleanupFn>(
				plugin.library->resolve("vyron_plugin_cleanup"));
			if (cleanup) {
				cleanup();
			}
			plugin.library->unload();
			plugin.library = nullptr;
		}
		plugin.loaded = false;
	}
	if (auto app = QApplication::instance()) {
		app->setStyleSheet(QString());
	}
}

void PluginManager::loadEnabledPlugins() {
	auto combinedStyles = QString();

	for (auto &plugin : _plugins) {
		if (!plugin.enabled) {
			continue;
		}

		if (!plugin.styleFile.isEmpty()) {
			const auto fullStylePath = QDir(plugin.dirPath).filePath(plugin.styleFile);
			auto styleFile = QFile(fullStylePath);
			if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
				combinedStyles += QTextStream(&styleFile).readAll() + u"\n"_q;
				styleFile.close();
			}
		}

		if (!plugin.libraryFile.isEmpty()) {
			const auto fullLibPath = QDir(plugin.dirPath).filePath(plugin.libraryFile);
			plugin.library = std::make_shared<QLibrary>(fullLibPath);
			if (plugin.library->load()) {
				const auto initFn = reinterpret_cast<PluginInitFn>(
					plugin.library->resolve("vyron_plugin_init"));
				if (initFn) {
					initFn(AppVersionStr);
				}
				plugin.loaded = true;
			}
		}
	}

	if (!combinedStyles.isEmpty()) {
		if (auto app = QApplication::instance()) {
			app->setStyleSheet(combinedStyles);
		}
	}
}

void PluginManager::reload() {
	unloadAll();
	scan();
	loadEnabledPlugins();
}

void PluginManager::setPluginEnabled(const QString &id, bool enabled) {
	if (enabled) {
		if (!_enabledIds.contains(id)) {
			_enabledIds.append(id);
		}
	} else {
		_enabledIds.removeAll(id);
	}
	saveConfig();

	for (auto &plugin : _plugins) {
		if (plugin.id == id) {
			plugin.enabled = enabled;
			break;
		}
	}
	reload();
}

bool PluginManager::isPluginEnabled(const QString &id) const {
	return _enabledIds.contains(id);
}

} // namespace Vyron
