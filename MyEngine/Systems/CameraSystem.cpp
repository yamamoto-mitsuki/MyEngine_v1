#include "CameraSystem.h"

#include <algorithm>
#include <memory>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"

namespace {
constexpr float kAspectRatio = 1280.0f / 720.0f;       // 縦横比（Game ビューの描画先・スプライトの基準と同じ 16:9）
constexpr float kMinFovY = 1.0f;                       // 視野角の下限（度）
constexpr float kMaxFovY = 179.0f;                     // 視野角の上限（度）。180度以上は透視投影で作れない
const Vector3 kDefaultPosition = {0.0f, 0.0f, -10.0f}; // Camera Component が無いときの位置（原点を正面から見る）

/// <summary>
/// CameraSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct CameraState {
	Camera camera;                  // ゲームの絵を撮るカメラ（GetMainCamera はいつもこれを返す）
	Handle<Entity> mainEntity;      // 今使っている Camera Component の Entity
	bool hasLoggedNoCamera = false; // 「Camera Component が無い」を知らせたか（続けて何度も出さない）
};
std::unique_ptr<CameraState> state;

// 写り方をカメラへ（視野角は度で持っているので、ラジアンに直して渡す）
void ApplyProjection(const CameraComponent& component) {
	const float fovY = std::clamp(component.fovY, kMinFovY, kMaxFovY) * kDegToRad;
	const float nearZ = (std::max)(component.nearZ, 0.001f);
	const float farZ = (std::max)(component.farZ, nearZ + 0.01f); // far が near より手前だと、何も映らない
	state->camera.Initialize(fovY, kAspectRatio, nearZ, farZ);    // Initialize の中で行列も作り直す
}

// Camera Component が無いときの、既定のカメラ
void UseDefaultCamera() {
	state->camera.SetTranslation(kDefaultPosition);
	state->camera.SetRotation({0.0f, 0.0f, 0.0f});
	state->camera.SetScale({1.0f, 1.0f, 1.0f});
	ApplyProjection(CameraComponent{});
}

// Camera の見せ方。Inspector とシーンファイルの両方がこの並びを使う（保存の名前はメンバ名）
void DescribeCamera(ComponentUI& ui, CameraComponent& value) {
	ui.Field("Field Of View###fovY", value.fovY, Range(kMinFovY, kMaxFovY).Tip("縦の視野角（度）。小さいほど望遠"));
	ui.Field("Near###nearZ", value.nearZ, AtLeast(0.001f).Tip("これより近い物は映らない"));
	ui.Field("Far###farZ", value.farZ, AtLeast(0.01f).Tip("これより遠い物は映らない"));
	ui.Field("Priority###priority", value.priority, Tip("有効な Camera が複数あるとき、大きい方を使う（切り替えはこの値か、Entity の有効・無効で）"));
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void CameraSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<CameraState>();
	UseDefaultCamera(); // 最初の Update より前（シーンの Initialize）に使われても、ちゃんとしたカメラを返せるように

	EntityManager::RegisterComponent<CameraComponent>();
	ComponentSerializer::Register<CameraComponent>("Camera", &DescribeCamera);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<CameraComponent>>("Camera", "Rendering3D", &DescribeCamera));
}

void CameraSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void CameraSystem::Update() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");

	// ===== 1. 使う Camera を選ぶ（有効な物の中で priority が一番大きい物。同じなら先に見つけた物）=====
	Handle<Entity> best;
	CameraComponent bestComponent;
	EntityManager::ForEach<CameraComponent>([&](Handle<Entity> entity, const CameraComponent& component) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (!best.IsValid() || component.priority > bestComponent.priority) {
			best = entity;
			bestComponent = component;
		}
	});
	state->mainEntity = best;

	// ===== 2. 無ければ既定のカメラ =====
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(best);
	if (transform == nullptr) {
		if (!state->hasLoggedNoCamera) {
			LogManager::Log("Camera Component が無いので、既定のカメラ（(0, 0, -10) から +Z を向く）で撮ります。Hierarchy の ＋ → Camera で置けます");
			state->hasLoggedNoCamera = true;
		}
		UseDefaultCamera();
		return;
	}
	state->hasLoggedNoCamera = false;

	// ===== 3. 位置・向きはワールド行列から（親の下に置いても、見えている場所・向きから撮る）=====
	// 大きさは写さない（ビュー行列に大きさが混ざると、絵が伸び縮みする）
	const Transform world = DecomposeAffineMatrix(transform->worldMatrix);
	state->camera.SetTranslation(world.translation);
	state->camera.SetRotation(world.rotation);
	state->camera.SetScale({1.0f, 1.0f, 1.0f});
	ApplyProjection(bestComponent);
}

//=============================================================================
// 取得
//=============================================================================
Camera* CameraSystem::GetMainCamera() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return &state->camera;
}

Handle<Entity> CameraSystem::GetMainCameraEntity() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return state->mainEntity;
}