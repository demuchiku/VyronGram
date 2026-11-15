#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#include <QLibrary>

namespace Vyron {

struct PluginInfo {
	QString id;
	QString name;
	QString version;
	QString author;
	QString description;
	QString dirPath;
	QString styleFile;
	QString libraryFile;
	bool enabled = false;
	bool loaded = false;
	std::shared_ptr<QLibrary> library;
};

class PluginManager {
public:
	static PluginManager &Instance();

	void init();
	void scan();
	void reload();
	void setPluginEnabled(const QString &id, bool enabled);
	[[nodiscard]] bool isPluginEnabled(const QString &id) const;

	[[nodiscard]] const std::vector<PluginInfo> &plugins() const {
		return _plugins;
	}
	[[nodiscard]] QString pluginsPath() const;

private:
	PluginManager() = default;

	void loadEnabledPlugins();
	void unloadAll();
	void saveConfig();
	void loadConfig();
	void ensureDefaultPluginExists();

	std::vector<PluginInfo> _plugins;
	QStringList _enabledIds;
};

} // namespace Vyron
