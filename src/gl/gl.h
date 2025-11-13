#pragma once

#include "gl_core_arb.h"

using PFNGETGLPROC = void* (const char*);

struct GL4API
{
	#include "gl_api.h"
};

void GetAPI4(GL4API* api, PFNGETGLPROC GetGLProc);
void InjectAPITracer4(GL4API* api);

extern GL4API api;
