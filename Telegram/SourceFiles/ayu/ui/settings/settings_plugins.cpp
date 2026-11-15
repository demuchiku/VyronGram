#include "ayu/ui/settings/settings_plugins.h"

#include "ayu/features/plugins/plugin_manager.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

void BuildPluginControls(SectionBuilder &builder) {
	const auto controller = builder.controller();

	builder.addSkip();
	builder.addButton({
		.id = u"vyron/openPluginsFolder"_q,
		.title = rpl::single(u"Open Plugins Folder"_q),
		.icon = { &st::menuIconShowInFolder },
		.onClick = [=] {
			const auto path = Vyron::PluginManager::Instance().pluginsPath();
			QDesktopServices::openUrl(QUrl::fromLocalFile(path));
		},
	});

	builder.addButton({
		.id = u"vyron/reloadPlugins"_q,
		.title = rpl::single(u"Reload Plugins"_q),
		.icon = { &st::menuIconRestartBot },
		.onClick = [=] {
			Vyron::PluginManager::Instance().reload();
			controller->showToast(u"Plugins reloaded"_q);
		},
	});
	builder.addSkip();
}

void BuildPluginList(SectionBuilder &builder) {
	const auto &plugins = Vyron::PluginManager::Instance().plugins();
	if (plugins.empty()) {
		builder.add([](const BuildContext &ctx) {
			v::match(ctx, [&](const WidgetContext &wctx) {
				const auto container = wctx.container;
				AddSkip(container);
				const auto label = container->add(
					object_ptr<Ui::FlatLabel>(
						container,
						rpl::single(u"No external plugins found in plugins folder.\nAdd plugins with plugin.json or .qss stylesheets."_q),
						st::boxLabel));
				label->setTextColorOverride(st::windowSubTextFg->c);
				AddSkip(container);
			});
		});
		return;
	}

	for (const auto &plugin : plugins) {
		const auto id = plugin.id;
		const auto nameWithVer = plugin.name + u" v"_q + plugin.version;
		builder.addToggle({
			.id = u"vyron/plugin_"_q + id,
			.title = rpl::single(nameWithVer),
			.getter = [=] {
				return Vyron::PluginManager::Instance().isPluginEnabled(id);
			},
			.setter = [=](bool enabled) {
				Vyron::PluginManager::Instance().setPluginEnabled(id, enabled);
			},
			.icon = { &st::menuIconManage },
		});
	}
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = VyronPlugins::Id(),
	.parentId = AyuMain::Id(),
	.title = u"Plugins"_q,
	.icon = &st::menuIconManage,
}, [](SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"External Plugins"_q));
	BuildPluginControls(builder);
	builder.addDivider();
	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Installed Plugins"_q));
	BuildPluginList(builder);
});

} // namespace

rpl::producer<QString> VyronPlugins::title() {
	return rpl::single(u"Plugins"_q);
}

VyronPlugins::VyronPlugins(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void VyronPlugins::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type VyronPluginsId() {
	return VyronPlugins::Id();
}

} // namespace Settings
