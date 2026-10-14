#pragma once
#include <memory>
#include "MyEngine/Graphics/IBL/CubeBakePass.h"
#include "MyEngine/Graphics/IBL/PrefilterPass.h"
#include "MyEngine/Graphics/IBL/BrdfLutPass.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"


class IBLBaker {
public:
	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class IBLBaker;
	};
	explicit IBLBaker(ConstructorKey) {} // 鍵を受け取るコンストラクタ

	static void Initialize();
	static void Finalize();
	static CubeBakePass& Equirect() { return instance_->equirectPass_; }
	static CubeBakePass& Irradiance() { return instance_->irradiancePass_; }
	static PrefilterPass& Prefilter() { return instance_->prefilterPass_; }
	static BrdfLutPass& BrdfLut() { return instance_->brdfLutPass_; }

private:
	// シングルトン
	friend struct std::default_delete<IBLBaker>;
	~IBLBaker() = default;
	IBLBaker(const IBLBaker&) = delete;
	IBLBaker& operator=(const IBLBaker&) = delete;
	static std::unique_ptr<IBLBaker> instance_;
	CubeBakePass equirectPass_;
	CubeBakePass irradiancePass_;
	PrefilterPass prefilterPass_;
	BrdfLutPass brdfLutPass_;
};