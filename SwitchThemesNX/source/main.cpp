#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <stdio.h>
#include <filesystem>
#include <stack>
#include <variant>

#include "UI/UIManagement.hpp"
#include "UI/UI.hpp"
#include "Platform/Platform.hpp"

#include "Pages/ThemePage.hpp"
#include "Pages/CfwSelectPage.hpp"
#include "Pages/UninstallPage.hpp"
#include "Pages/NcaDumpPage.hpp"
#include "Pages/TextPage.hpp"
#include "Pages/ExternalInstallPage.hpp"
#include "ViewFunctions.hpp"
#include "SwitchThemesCommon/SwitchThemesCommon.hpp"
#include "SwitchThemesCommon/NXTheme.hpp"
#include "Pages/RemoteInstallPage.hpp"
#include "Pages/SettingsPage.hpp"
#include "Pages/RebootPage.cpp"

#include "SwitchTools/PatchMng.hpp"

//#define DEBUG
using namespace std;

static inline void ImguiBindController()
{
	ImGuiIO& io = ImGui::GetIO();

	NAV_DOWN	= (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_DOWN)	|| StickAsButton(1) > .3f	|| StickAsButton(3) > .3f );
	NAV_UP		= (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_UP)		|| StickAsButton(1) < -.3f	|| StickAsButton(3) < -.3f);
	NAV_LEFT	= (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT)	|| StickAsButton(0) < -.3f	|| StickAsButton(2) < -.3f);
	NAV_RIGHT	= (KeyPressed(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT)	|| StickAsButton(0) > .3f	|| StickAsButton(2) > .3f );

	io.NavInputs[ImGuiNavInput_DpadDown]	= NAV_DOWN	? 1.0f : 0;
	io.NavInputs[ImGuiNavInput_DpadUp]		= NAV_UP	? 1.0f : 0;
	io.NavInputs[ImGuiNavInput_DpadLeft]	= NAV_LEFT	? 1.0f : 0;
	io.NavInputs[ImGuiNavInput_DpadRight]	= NAV_RIGHT	? 1.0f : 0;

	io.NavInputs[ImGuiNavInput_Activate] = KeyPressed(GLFW_GAMEPAD_BUTTON_A);
	io.NavInputs[ImGuiNavInput_Cancel] = KeyPressed(GLFW_GAMEPAD_BUTTON_B);

	io.NavActive = true;
	io.NavVisible = true;
}

static bool IsRendering = false;
stack<IUIControlObj*> views;
bool doPopPage = false;

IUIControlObj *ViewObj = nullptr;

void PopPage()
{
	doPopPage = true;
}

static void _PopPage()
{
	doPopPage = false;
	delete views.top();
	views.pop();
	if (views.size() == 0)
	{
		//Dialog("Error: Can't pop last page");
		return;
	}
	ViewObj = views.top();
}

void PushPage(IUIControlObj* page) //All pages must be dynamically allocated
{
	views.push(page);
	ViewObj = page;
}

vector<function<void()>> DeferredFunctions;
void PushFunction(const std::function<void()>& fun)
{
	DeferredFunctions.push_back(fun);
}

static void ExecuteDeferredFunctions() 
{
	auto vec = DeferredFunctions;
	DeferredFunctions.clear();
	for (auto& fun : vec)
		fun();
}

void PushPageBlocking(IUIControlObj* page)
{
	if (IsRendering)
	{
		LOGf("Attempted to push a blocking page while rendering");
		PushFunction([page]() {PushPageBlocking(page); });
		return;
	}

	PushPage(page);
	while (AppMainLoop() && ViewObj)
	{
		PlatformGetInputs();
		ImguiBindController();
		PlatformImguiBinds();
		
		IUIControlObj* CurObj = ViewObj;

		IsRendering = true;
		UiStartFrame();
		CurObj->Render(0,0);
		UiEndFrame();
		IsRendering = false;

		if (CurObj == ViewObj)
			CurObj->Update();
		
		ExecuteDeferredFunctions();
		if (doPopPage)
		{
			_PopPage();
			break;
		}

		PlatformSleep(1 / 30.0f * 1000);
	}
}

void Dialog(const string &msg)
{
	PushPage(new DialogPage(msg));
}

//TODO less hacky way
void DialogBlocking(const string &msg)
{
	PushPageBlocking(new DialogPage(msg));
}

void DisplayLoading(const string &msg)
{
	UiStartFrame();
	LoadingOverlay o(msg);
	o.Render(0, 0);
	UiEndFrame();
}

#ifdef DEBUG
double previousTime = glfwGetTime();
int frameCount = 0;
int fpsValue = 0;

static void calcFPS() 
{
	double currentTime = glfwGetTime();
	frameCount++;

	if (currentTime - previousTime >= 1.0)
	{
		fpsValue = frameCount;

		frameCount = 0;
		previousTime = currentTime;
	}
	ImGui::Text("FPS %d", fpsValue);
	for (int i = 0; i < 6; i++)
	{
		ImGui::Text("pad[%d] = %d %f %f ", i, (int)(StickAsButton(i) != 0), gamepad.axes[i], OldGamepad.axes[i]);
	}
}
#endif

static void MainLoop()
{
	while (AppMainLoop() && ViewObj)
	{
		PlatformGetInputs();
		ImguiBindController();
		PlatformImguiBinds();

		//A control may push a page either in the render or the update function.
		IUIControlObj* CurObj = ViewObj;

		IsRendering = true;
		UiStartFrame();		
		CurObj->Render(0,0);
#ifdef DEBUG
		calcFPS();
#endif
		UiEndFrame();
		IsRendering = false;

		if (CurObj == ViewObj)
			CurObj->Update();
		
		ExecuteDeferredFunctions();
		if (doPopPage)
			_PopPage();

		PlatformSleep(1 / 30.0f * 1000);
	}
}

class QuitPage : public IPage
{
	public:
		QuitPage()
		{
			Name = "Quit";
		}	
		
		void Render(int X, int Y) override {}
		
		void Update() override
		{
			SetAppShouldClose();
		}
};

void ShowFirstTimeHelp(bool WelcomeScr)
{	
//these are shown from the last to the first
	Dialog("You can find some themes on the /r/NXThemes subreddit and in the Qcean Discord server (invite: CUnHJgb) where you can also ask for support. \n\n"
"To make your own themes download the windows app at: https://git.io/fpxAS\n"
"Or use the online theme editor at: https://exelix11.github.io/SwitchThemeInjector/v2\n"
"\n"
"That's all, have fun with custom themes :)");
	Dialog("Altough .nxtheme files can be INSTALLED on every firmware you still have to uninstall any theme before updating, this is because the nxtheme gets converted to an SZS when it's installed.\nAfter the update you will be able to reinstall all your themes in .nxtheme format usually without any compatibility issue.\n"
"(Please note that some features such as custom Settings page are available only on >= 6.X firmwares)");
	Dialog("SZS files unfortunately are illegal to share as they contain copyrighted data, that's why this tool also supports .nxtheme files. These work just like SZS files but they can be freely shared and most importantly installed on every firmware.");
	Dialog("Custom themes are custom SZS files that replace some files of the home menu, these files are firmware-dependent, which means that if you update your firmware while having a custom theme installed your console may not boot anymore until you manually remove the custom theme.\n\n"
"To remove a custom theme you either boot your CFW without LayeredFS and use this tool to uninstall it or manually delete the '0100000000001000' and '0100000000001013' folders in 'SDcard/<your cfw folder>/titles'.\n\n"
"Custom themes CANNOT brick your console because they're installed only on the SDcard");
	if (WelcomeScr)
		Dialog("Welcome to NXThemes Installer " + VersionString + "!\n\nThese pages contains some important informations, it's recommended to read them carefully.\nThis will only show up once, you can read it again from the Credits tab." );
}

// Note that CfwFolder is set after the constructor of any page pushed before CheckCFWDir is called, CfwFolder shouldn't be used until the theme is actually being installed
static void CheckCFWDir()
{
	auto f = fs::SearchCfwFolders();
	if (f.size() != 1)
		PushPageBlocking(new CfwSelectPage(f));
}

static vector<string> GetArgsInstallList(int argc, char** argv)
{
	if (argc <= 1) return {};

	vector<string> Args(argc - 1);
	for (size_t i = 0; i < Args.size(); i++)
		Args[i] = string(argv[i + 1]);

	//Appstore-style args
	if (StrStartsWith(Args[0], "installtheme="))
	{
		string key = "installtheme=";
		string pathss;
		std::vector<std::string> paths;
		for (auto argvs : Args)
		{
			auto pos = argvs.find(key);
			size_t index;
			while (true)
			{
				index = argvs.find("(_)");
				if (index == std::string::npos) break;
				argvs.replace(index, 3, " ");
			}
			if (pos != std::string::npos)
				pathss = argvs.substr(pos + 13);

			if (!pathss.empty())
			{
				string path;
				stringstream stream(pathss);
				while (getline(stream, path, ',')) {
					paths.push_back(path);
				}
			}
		}
		return paths;
	}
	else //File args from nx-hbloader
	{
		vector<string> res;

		for (auto& s : Args)
			if (filesystem::exists(s) && filesystem::is_regular_file(s))
				res.push_back(move(s));

		return res;
	}
}	

std::string SystemVer = "";
static void SetupSysVer()
{
#if __SWITCH__
	setsysInitialize();
	SetSysFirmwareVersion firmware;
	auto res = setsysGetFirmwareVersion(&firmware);
	if (R_FAILED(res))
	{
		setsysExit();
		DialogBlocking("Could not get sys ver res=" + to_string(res));
		return;
	}
	HOSVer = { firmware.major,firmware.minor,firmware.micro };
	setsysExit();
#else 
	HOSVer = { 9,0,0 };
#endif
	if (HOSVer.major <= 5)
	{
		ThemeTargetToName = ThemeTargetToName5X;
		ThemeTargetToFileName = ThemeTargetToFileName6X; //The file names are the same
	}
	else //6.X
	{
		ThemeTargetToName = ThemeTargetToName6X;
		ThemeTargetToFileName = ThemeTargetToFileName6X;
	}
	SystemVer = to_string(HOSVer.major) + "." + to_string(HOSVer.minor) + "." + to_string(HOSVer.micro);
}

int main(int argc, char **argv)
{
	PlatformInit();

	if (!UIMNG::InitUI())
	{
		PlatformExit();
		return -1;
	}
	PlatformAfterInit();

	SetupSysVer();
	DisplayLoading("Loading system info...");
	
	bool ThemesFolderExists = fs::CheckThemesFolder();
	NcaDumpPage::CheckHomeMenuVer();
	CheckCFWDir();
	const char* PatchErrorMsg = PatchMng::EnsureInstalled();

	if (
#ifdef __SWITCH__
		envHasArgv() &&
#endif
		argc > 1)
	{
		auto paths = GetArgsInstallList(argc,argv);
		if (paths.size() == 0)
			goto APP_QUIT;

		PushPage(new ExternalInstallPage(paths));	
		MainLoop();
		
		goto APP_QUIT;
	}	

	{
		TabRenderer *t = new TabRenderer();
		PushPage(t);
		
		if (!ThemesFolderExists)
			ShowFirstTimeHelp(true);
		
		TextPage* PatchFailedWarning = nullptr;
		if (PatchErrorMsg)
		{
			PatchFailedWarning = new TextPage("Warning", PatchErrorMsg);
			t->AddPage(PatchFailedWarning);
		}

		DisplayLoading("Loading theme list...");
		auto ThemeFiles = fs::GetThemeFiles();
		ThemesPage *p = new ThemesPage(ThemeFiles);
		t->AddPage(p);
		UninstallPage *up = new UninstallPage();
		t->AddPage(up);
		NcaDumpPage *dp = new NcaDumpPage();
		t->AddPage(dp);
		RemoteInstallPage *rmi = new RemoteInstallPage();
		t->AddPage(rmi);
		SettingsPage *sf = new SettingsPage();
		t->AddPage(sf);
		CreditsPage *credits = new CreditsPage();
		t->AddPage(credits);
		RebootPage *reboot = new RebootPage();
		t->AddPage(reboot);
		QuitPage *q = new QuitPage();
		t->AddPage(q);
		
		MainLoop();
		
		if (PatchFailedWarning)
			delete PatchFailedWarning;
		delete p;
		delete up;
		delete dp;
		delete rmi;
		delete sf;
		delete credits;
		delete q;
	}
	
APP_QUIT:

	while (views.size() != 0)
	{
		delete views.top();
		views.pop();
	}

	UIMNG::ExitUI();
	PlatformExit();
	
    return 0;
}