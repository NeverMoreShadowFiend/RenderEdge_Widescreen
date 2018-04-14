#include <windows.h>

#include <cmath> 
#include "War3Versions.h"
#include "detours.h"
#include "basic_searcher.h"

#pragma comment( lib, "detours.lib" )

HWND g_hWnd;
uintptr_t address_GameBase = 0;
uintptr_t address_CreateMatrixPerspectiveFov = 0;
uintptr_t address_BuildHPBars = 0;
float g_fWideScreenMul = 1.0f;

base::warcraft3::basic_searcher war3_searcher(GetModuleHandleW(L"game.dll"));


template <typename ReturnType, typename FPType, typename A0, typename A1, typename A2, typename A3>
inline ReturnType fast_call(FPType fp, A0 a0, A1 a1, A2 a2, A3 a3)
{
	typedef ReturnType(__fastcall *TFPAeroFunction)(A0 a0, A1 a1, A2 a2, A3 a3);

	return ((TFPAeroFunction)fp)(a0, a1, a2, a3);
}


void __fastcall CreateMatrixPerspectiveFov_proxy(uint32_t outMatrix, uint32_t unused, float fovY, float aspectRatio, float nearZ, float farZ)
{
	RECT r;
	if (GetWindowRect(g_hWnd, &r))
	{
		float width = float(r.right - r.left);
		float rHeight = 1.0f / (r.bottom - r.top);
		g_fWideScreenMul = width * rHeight * 0.75f; // (width / height) / (4.0f / 3.0f)
	}

	float yScale = 1.0f / tan(fovY * 0.5f / sqrt(aspectRatio * aspectRatio + 1.0f));
	float xScale = yScale / (aspectRatio * g_fWideScreenMul);

	*(float*)(outMatrix) = xScale;
	*(float*)(outMatrix + 16) = 0.0f;
	*(float*)(outMatrix + 32) = 0.0f;
	*(float*)(outMatrix + 48) = 0.0f;

	*(float*)(outMatrix + 4) = 0.0f;
	*(float*)(outMatrix + 20) = yScale;
	*(float*)(outMatrix + 36) = 0.0f;
	*(float*)(outMatrix + 52) = 0.0f;

	*(float*)(outMatrix + 8) = 0.0f;
	*(float*)(outMatrix + 24) = 0.0f;
	*(float*)(outMatrix + 40) = (nearZ + farZ) / (farZ - nearZ);
	*(float*)(outMatrix + 56) = (-2.0f * nearZ * farZ) / (farZ - nearZ);

	*(float*)(outMatrix + 12) = 0.0f;
	*(float*)(outMatrix + 28) = 0.0f;
	*(float*)(outMatrix + 44) = 1.0f;
	*(float*)(outMatrix + 60) = 0.0f;
}


void __fastcall BuildHPBars_proxy(uint32_t a1, uint32_t unused, uint32_t a2, uint32_t a3)
{
	fast_call<void>(address_BuildHPBars, a1, unused, a2, a3);

	uint32_t pHPBarFrame = *((uint32_t*)a1 + 3);
	if (pHPBarFrame)
		*((float*)pHPBarFrame + 22) /= g_fWideScreenMul;
}



bool inline_install(uintptr_t* pointer_ptr, uintptr_t detour)
{
	LONG status;
	if ((status = DetourTransactionBegin()) == NO_ERROR)
	{
		if ((status = DetourUpdateThread(GetCurrentThread())) == NO_ERROR)
		{
			if ((status = DetourAttach((PVOID*)pointer_ptr, (PVOID)detour)) == NO_ERROR)
			{
				if ((status = DetourTransactionCommit()) == NO_ERROR)
				{
					return true;
				}
			}
		}
		DetourTransactionAbort();
	}
	return false;
}


Version GetGameVersion()
{
	static const char warcraft3_version_string[] = "Warcraft III (build ";

	uintptr_t ptr;
	size_t size = sizeof(warcraft3_version_string) - 1;
	ptr = war3_searcher.search_string_ptr(warcraft3_version_string, size);

	if (!ptr)
		return Version::unknown;

	uint32_t n = 0;
	ptr += size;
	while (isdigit(*(uint8_t*)ptr))
	{
		n = n * 10 + *(uint8_t*)ptr - '0';
		ptr++;
	}

	return static_cast<Version>(n);
}


BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*pReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);

		address_GameBase = (uintptr_t)GetModuleHandleW(L"Game.dll");
		g_hWnd = GetActiveWindow();

		Version gameVersion = GetGameVersion();
		switch (gameVersion)
		{
		case Version::v128f:
			address_CreateMatrixPerspectiveFov = 0x1554F0;
			address_BuildHPBars = 0x3C6700;
			break;
		case Version::v128e:
			address_CreateMatrixPerspectiveFov = 0x1553B0;
			address_BuildHPBars = 0x3C6560;
			break;
		case Version::v128c:
			address_CreateMatrixPerspectiveFov = 0x12B730;
			address_BuildHPBars = 0x39C670;
			break;
		case Version::v128a:
			address_CreateMatrixPerspectiveFov = 0x129150;
			address_BuildHPBars = 0x399F20;
			break;
		case Version::v127b:
			address_CreateMatrixPerspectiveFov = 0x126E30;
			address_BuildHPBars = 0x3925F0;
			break;
		case Version::v127a:
			address_CreateMatrixPerspectiveFov = 0x0D31D0;
			address_BuildHPBars = 0x374E60;
			break;
		case Version::v126a:
			address_CreateMatrixPerspectiveFov = 0x7B66F0;
			address_BuildHPBars = 0x379A30;
			break;
		case Version::v124e:
			address_CreateMatrixPerspectiveFov = 0x7B6E90;
			address_BuildHPBars = 0x37A570;
			break;
		case Version::v123a:
			address_CreateMatrixPerspectiveFov = 0x7A7BA0;
			address_BuildHPBars = 0x37A3F0;
			break;
		default:
			MessageBoxW(0, L"Error! Your WarCraft version is not supported.", L"RenderEdge_Widescreen.mix", 0);
			break;
		}

		if (address_CreateMatrixPerspectiveFov)
		{
			address_CreateMatrixPerspectiveFov += address_GameBase;
			inline_install(&address_CreateMatrixPerspectiveFov, (uintptr_t)CreateMatrixPerspectiveFov_proxy);
		}

		if (address_BuildHPBars)
		{
			address_BuildHPBars += address_GameBase;
			inline_install(&address_BuildHPBars, (uintptr_t)BuildHPBars_proxy);
		}
	}

	return TRUE;
}