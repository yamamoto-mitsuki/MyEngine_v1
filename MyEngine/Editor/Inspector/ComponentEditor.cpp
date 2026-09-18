#include "ComponentEditor.h"

#include <string_view>

namespace {
std::vector<std::unique_ptr<ComponentEditor>> editors; // 登録された順
}

void ComponentEditorRegistry::Register(std::unique_ptr<ComponentEditor> editor) {
	for (const std::unique_ptr<ComponentEditor>& registered : editors) {
		if (std::string_view(registered->GetName()) == editor->GetName()) {
			return;
		}
	}
	editors.push_back(std::move(editor));
}

const std::vector<std::unique_ptr<ComponentEditor>>& ComponentEditorRegistry::GetAll() { return editors; }