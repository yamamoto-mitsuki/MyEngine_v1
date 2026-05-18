#pragma once
#include <string>
#include <memory>

class IScene {
public:
	virtual ~IScene() = default;
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual void Finalize() = 0;
	virtual std::unique_ptr<IScene> NextScene() { return nullptr; }

	// WindowManagerが自動でセットする
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }
	const std::wstring& GetWindowTitle() const { return windowTitle_; }

protected:
	std::wstring windowTitle_ = L"Title";
};