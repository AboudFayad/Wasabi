#pragma once

#include "Wasabi/Renderers/WRenderStage.h"
#include "Wasabi/Renderers/Common/WRenderFragment.h"

class WSpritesRenderStage : public WRenderStage {
	WSpritesRenderFragment* m_spritesFragment;

public:
	WSpritesRenderStage(class Wasabi* const app, bool backbuffer = false);

	virtual WError Initialize(std::vector<WRenderStage*>& previousStages, uint32_t width, uint32_t height);
	virtual WError Render(class WRenderer* renderer, class WRenderTarget* rt, uint32_t filter);
	virtual void Cleanup();
	virtual WError Resize(uint32_t width, uint32_t height);
};
