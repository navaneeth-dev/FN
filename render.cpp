#include "stdafx.h"
#include "hooks.h"
#include "imgui/imgui_impl_dx11.h"

IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int ActorCount = 0;
uintptr_t UworldAddress = 0;
vector<ULONGLONG> PlayerPawns;
char* PlayerName = (char*)"Nothing";
int pwnId = 0;
static float width = 0;
static float height = 0;
static HWND hWnd = 0;
vector<FVector> PawnLocations;

namespace Render {
	BOOLEAN showMenu = FALSE;

	WNDPROC WndProcOriginal = nullptr;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* immediateContext = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	HRESULT(*PresentOriginal)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) = nullptr;
	HRESULT(*ResizeOriginal)(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) = nullptr;

	ImGuiWindow& BeginScene() {
		ImGui_ImplDX11_NewFrame();
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		ImGui::Begin(XorStr("##scene").c_str(), nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);

		auto& io = ImGui::GetIO();
		ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);

		return *ImGui::GetCurrentWindow();
	}

	VOID EndScene(ImGuiWindow& window) {
		window.DrawList->PushClipRectFullScreen();
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.17f, 0.18f, 0.2f, 1.0f));

		if (showMenu) {
			ImGui::Begin(XorStr("##menu").c_str(), reinterpret_cast<bool*>(true), ImGuiWindowFlags_NoCollapse);
			ImGui::SetWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			ImGui::Text(XorStr("GayNiteFN").c_str());

			ImGui::Text(XorStr("Aimbot:").c_str());
			ImGui::Checkbox(XorStr("Memory Aimbot").c_str(), &settings.Aimbot);
			ImGui::Checkbox(XorStr("Decrypt").c_str(), &settings.Decrypt);
			ImGui::Checkbox(XorStr("Loop").c_str(), &settings.Loop);

			ImGui::Text(XorStr("Visuals:").c_str());
			ImGui::Checkbox(XorStr("BOX ESP").c_str(), &settings.ESP.Players);
			ImGui::Checkbox(XorStr("Aimbot FOV").c_str(), &settings.FOV);
			ImGui::Checkbox(XorStr("Players Around").c_str(), &settings.PlayersAround);
			if (settings.FOV) {
				ImGui::SliderFloat(XorStr("aim-fov##slider").c_str(), &settings.FOVSize, 0.0f, 1000.0f, XorStr("%.2f").c_str());
			}

			ImGui::Text(XorStr("Debug:").c_str());
			if (!settings.Debug) {
				ImGui::Text(XorStr("Actor Count -> %lu\n").c_str(), ActorCount);
				ImGui::Text(XorStr("Pawns -> %lu\n").c_str(), PlayerPawns.size());
				ImGui::Text(XorStr("PawnID -> %lu\n").c_str(), pwnId);
				char buffer[40] = { 0 };
				sprintf_s(buffer, "%llx\n", UworldAddress);
				ImGui::InputText(XorStr("UWorld").c_str(), buffer, 40);
			}

			ImGui::End();
		}

		ImGui::PopStyleColor();

		ImGui::Render();
	}

	__declspec(dllexport) LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (msg == WM_KEYUP && (wParam == VK_INSERT || (showMenu && wParam == VK_ESCAPE))) {
			showMenu = !showMenu;
			ImGui::GetIO().MouseDrawCursor = showMenu;

			/*if (!showMenu) {
				SettingsHelper::SaveSettings();
			}*/
		}
		else if (msg == WM_QUIT && showMenu) {
			// SettingsHelper::SaveSettings();
			ExitProcess(0);
		}

		if (showMenu) {
			ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
			return TRUE;
		}

		return CallWindowProc(WndProcOriginal, hwnd, msg, wParam, lParam);
	}

	__declspec(dllexport) HRESULT PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
		if (!device) {
			swapChain->GetDevice(__uuidof(device), reinterpret_cast<PVOID*>(&device));
			device->GetImmediateContext(&immediateContext);

			ID3D11Texture2D* renderTarget = nullptr;
			swapChain->GetBuffer(0, __uuidof(renderTarget), reinterpret_cast<PVOID*>(&renderTarget));
			device->CreateRenderTargetView(renderTarget, nullptr, &renderTargetView);
			renderTarget->Release();

			ID3D11Texture2D* backBuffer = 0;
			swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (PVOID*)&backBuffer);
			D3D11_TEXTURE2D_DESC backBufferDesc = { 0 };
			backBuffer->GetDesc(&backBufferDesc);

			hWnd = FindWindow(XorStr(L"UnrealWindow").c_str(), XorStr(L"Fortnite  ").c_str());
			if (!width) {
				WndProcOriginal = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));
			}

			width = (float)backBufferDesc.Width;
			height = (float)backBufferDesc.Height;
			backBuffer->Release();

			HWND targetWindow = 0;
			EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
				DWORD pid = 0;
				GetWindowThreadProcessId(hWnd, &pid);
				if (pid == GetCurrentProcessId()) {
					*reinterpret_cast<HWND*>(lParam) = hWnd;
					return FALSE;
				}

				return TRUE;
			}, reinterpret_cast<LPARAM>(&targetWindow));

			ImGui_ImplDX11_Init(targetWindow, device, immediateContext);
			ImGui_ImplDX11_CreateDeviceObjects();
		}

		immediateContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

		auto& window = BeginScene();

		if (settings.FOV) {
			window.DrawList->AddCircle(ImVec2(width / 2, height / 2), settings.FOVSize, ImGui::GetColorU32({ 0.0f, 0.0f, 0.0f, 1.0f }), 32, 2.0f);
		}

		do {
			auto world = *Offsets::uWorld;
			if (!valid_pointer((void*)world)) break;

			UworldAddress = (ULONGLONG)world;

			ULONGLONG PersistentLevel = *(ULONGLONG*)((ULONGLONG)world + 0x30);
			if (!valid_pointer((void*)PersistentLevel)) break;

			ActorCount = *(UINT32*)(PersistentLevel + Offsets::Engine::Level::AActors + sizeof(ULONGLONG));
			if (ActorCount == 0) break;

			auto gameInstance = ReadPtr((ULONGLONG)world, Offsets::Engine::World::OwningGameInstance);
			if (!valid_pointer((void*)gameInstance)) break;

			auto localPlayers = ReadPtr(gameInstance, Offsets::Engine::GameInstance::LocalPlayers);
			if (!valid_pointer((void*)localPlayers)) break;

			auto localPlayer = ReadPtr(localPlayers, 0);
			if (!valid_pointer((void*)localPlayer)) break;

			auto localPlayerController = ReadPtr(localPlayer, Offsets::Engine::Player::PlayerController);
			if (!valid_pointer((void*)localPlayerController)) break;

			auto AcknowledgedPawn = ReadPtr(localPlayerController, Offsets::Engine::PlayerController::AcknowledgedPawn);
			if (!valid_pointer((void*)AcknowledgedPawn)) break;
			
			PlayerPawns.clear();
			auto ActorList = ReadPtr(PersistentLevel, Offsets::Engine::Level::AActors);

			if (!valid_pointer((void*)ActorList)) break;

			if (settings.Loop) {
				for (unsigned int pw = 0; pw < ActorCount; pw++) {
					auto Actor = ReadPtr(ActorList, pw * sizeof(ULONGLONG));

					if (!valid_pointer((void*)Actor) || Actor == AcknowledgedPawn)
						continue;

					char buffer[100] = { 0 };
					if (settings.Decrypt) {
						int ActorId = *(int*)(Actor + 0x18);
						Utils::decrypt_name(ActorId, buffer, 64);
					}

					if (strstr(buffer, "PlayerPawn_Athena_C") || strstr(buffer, "PlayerPawn_Athena_Phoebe_C") || strstr(buffer, "PlayerPawn_French")) {
						PlayerPawns.push_back(Actor);
					}
				}
			}

			if (settings.PlayersAround) {
				char EnemiesBuffer[20];
				sprintf_s(EnemiesBuffer, "Enemies: %u", PlayerPawns.size());
				window.DrawList->AddText(ImVec2(width / 2, 100), ImGui::GetColorU32({ 1.0f, 0.0f, 0.0f, 1.0f }), EnemiesBuffer);
			}

			/*
			for (auto pawn : PlayerPawns)
			{
				auto RootComp = ReadPtr(pawn, Offsets::Engine::Actor::RootComponent);
				if (!valid_pointer(RootComp)) continue;
				FVector pawnPosition = *(FVector*)(RootComp + Offsets::Engine::SceneComponent::RelativeLocation);
				PawnLocations.push_back(pawnPosition);
			}*/

		} while (FALSE);

		Render::EndScene(window);

		return PresentOriginal(swapChain, syncInterval, flags);
	}

	__declspec(dllexport) HRESULT ResizeHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) {
		ImGui_ImplDX11_Shutdown();
		renderTargetView->Release();
		immediateContext->Release();
		device->Release();
		device = nullptr;

		return ResizeOriginal(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
	}

	BOOLEAN Initialize() {
		IDXGISwapChain* swapChain = nullptr;
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		auto                 featureLevel = D3D_FEATURE_LEVEL_11_0;

		DXGI_SWAP_CHAIN_DESC sd = { 0 };
		sd.BufferCount = 1;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.OutputWindow = FindWindowA(XorStr("UnrealWindow").c_str(), XorStr("Fortnite  ").c_str());
		sd.SampleDesc.Count = 1;
		sd.Windowed = TRUE;

		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, &featureLevel, 1, D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context))) {
			MessageBox(0, XorStr(L"Failed to create D3D11 device and swap chain").c_str(), XorStr(L"Failure").c_str(), MB_ICONERROR);
			return FALSE;
		}

		auto table = *reinterpret_cast<PVOID**>(swapChain);
		auto present = table[8];
		auto resize = table[13];

		context->Release();
		device->Release();
		swapChain->Release();

		MH_CreateHook(present, PresentHook, reinterpret_cast<PVOID*>(&PresentOriginal));
		MH_EnableHook(present);

		MH_CreateHook(resize, ResizeHook, reinterpret_cast<PVOID*>(&ResizeOriginal));
		MH_EnableHook(resize);

		MH_CreateHook(&GetWindowTextA, GetWindowTextAHook, reinterpret_cast<PVOID*>(&GetWindowTextAOriginal));
		MH_EnableHook(&GetWindowTextA);

		MH_CreateHook(&GetWindowTextW, GetWindowTextWHook, reinterpret_cast<PVOID*>(&GetWindowTextWOriginal));
		MH_EnableHook(&GetWindowTextW);

		MH_CreateHook(&CreateFileA, CreateFileAHook, reinterpret_cast<PVOID*>(&CreateFileAOriginal));
		MH_EnableHook(&CreateFileA);

		MH_CreateHook(&CreateFileW, CreateFileWHook, reinterpret_cast<PVOID*>(&CreateFileWOriginal));
		MH_EnableHook(&CreateFileW);

		MH_CreateHook(&DeviceIoControl, DeviceIoControlHook, reinterpret_cast<PVOID*>(&DeviceIoControlOriginal));
		MH_EnableHook(&DeviceIoControl);

		MH_CreateHook(&SetupDiGetDevicePropertyW, SetupDiGetDevicePropertyWHook, reinterpret_cast<PVOID*>(&SetupDiGetDevicePropertyWOriginal));
		MH_EnableHook(&SetupDiGetDevicePropertyW);

		MH_CreateHook(&SetupDiGetDeviceRegistryPropertyW, SetupDiGetDeviceRegistryPropertyWHook, reinterpret_cast<PVOID*>(&SetupDiGetDeviceRegistryPropertyWOriginal));
		MH_EnableHook(&SetupDiGetDeviceRegistryPropertyW);

		MH_CreateHook(&SetupDiGetDeviceInstanceIdW, SetupDiGetDeviceInstanceIdWHook, reinterpret_cast<PVOID*>(&SetupDiGetDeviceInstanceIdWOrignal));
		MH_EnableHook(&SetupDiGetDeviceInstanceIdW);

		return TRUE;
	}
}
