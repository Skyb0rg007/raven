#pragma once

#include "presentation.h"
#include <memory>

namespace raven {

class XcbPresentation {
public:
	XcbPresentation();
	~XcbPresentation();
	XcbPresentation(const XcbPresentation &)=delete;
	XcbPresentation &operator=(const XcbPresentation &)=delete;
	DesktopPresentation callbacks();

private:
	struct State;
	std::unique_ptr<State> state;
};

}
