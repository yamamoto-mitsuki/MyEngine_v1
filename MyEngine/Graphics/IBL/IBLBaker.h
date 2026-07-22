#pragma once
#include "MyEngine/Graphics/IBL/EquirectToCubePass.h"

class IBLBaker {
public:
	static void Initialize();
	static void Finalize();
	static EquirectToCubePass& Equirect() { return instance_->equirectPass_; }


private:
	static IBLBaker* instance_;
	EquirectToCubePass equirectPass_;
};