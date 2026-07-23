#pragma once
#include "MyEngine/Graphics/IBL/IBLIncludes.h"

class IBLBaker {
public:
	static void Initialize();
	static void Finalize();
	static CubeBakePass& Equirect() { return instance_->equirectPass_; }
	static CubeBakePass& Irradiance() { return instance_->irradiancePass_; }
	static PrefilterPass& Prefilter() { return instance_->prefilterPass_; }

private:
	static IBLBaker* instance_;
	CubeBakePass equirectPass_;
	CubeBakePass irradiancePass_;
	PrefilterPass prefilterPass_;
};