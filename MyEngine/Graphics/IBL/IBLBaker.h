#pragma once
#include "MyEngine/Graphics/IBL/CubeBakePass.h"

class IBLBaker {
public:
	static void Initialize();
	static void Finalize();
	static CubeBakePass& Equirect() { return instance_->equirectPass_; }
	static CubeBakePass& Irradiance() { return instance_->irradiancePass_; }


private:
	static IBLBaker* instance_;
	CubeBakePass equirectPass_;
	CubeBakePass irradiancePass_;
};