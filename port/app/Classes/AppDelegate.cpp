#include "AppDelegate.h"

#include "cocos2d.h"
#include "SimpleAudioEngine.h"
#include "ScriptingCore.h"
#include "generated/jsb_cocos2dx_auto.hpp"
#include "generated/jsb_cocos2dx_extension_auto.hpp"
#include "generated/jsb_cocos2dx_studio_auto.hpp"
#include "jsb_cocos2dx_extension_manual.h"
#include "jsb_cocos2dx_studio_manual.h"
#include "cocos2d_specifics.hpp"
#include "js_bindings_chipmunk_registration.h"
#include "js_bindings_system_registration.h"
#include "js_bindings_ccbreader.h"
#include "jsb_opengl_registration.h"
#include "XMLHTTPRequest.h"
#include "jsb_websocket.h"
#include "PcBindings.h"
#include <windows.h>

USING_NS_CC;
using namespace CocosDenshion;

static std::string exeDir()
{
    char path[MAX_PATH] = "";
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string dir(path);
    return dir.substr(0, dir.find_last_of("\\/"));
}

AppDelegate::AppDelegate()
{
}

AppDelegate::~AppDelegate()
{
    CCScriptEngineManager::purgeSharedManager();
}

bool AppDelegate::applicationDidFinishLaunching()
{
    CCDirector* director = CCDirector::sharedDirector();
    director->setOpenGLView(CCEGLView::sharedOpenGLView());
    director->setDisplayStats(false);
    director->setAnimationInterval(1.0 / 60);

    // Working directory is the game data folder (set by the launcher). The APK keeps scripts
    // and UI under UI/, sprite sheets under */resources-large/, and the engine's JS glue
    // (jsb*.js, staged next to the exe) is found through the exe folder.
    CCFileUtils* fu = CCFileUtils::sharedFileUtils();
    fu->addSearchPath("UI");
    fu->addSearchPath(exeDir().c_str());
    std::vector<std::string> resolutions;
    resolutions.push_back("resources-large");
    resolutions.push_back("");
    fu->setSearchResolutionsOrder(resolutions);

    ScriptingCore* sc = ScriptingCore::getInstance();
    sc->addRegisterCallback(register_all_cocos2dx);
    sc->addRegisterCallback(register_all_cocos2dx_extension);
    sc->addRegisterCallback(register_all_cocos2dx_extension_manual);
    sc->addRegisterCallback(register_cocos2dx_js_extensions);
    sc->addRegisterCallback(register_all_cocos2dx_studio);
    sc->addRegisterCallback(register_all_cocos2dx_studio_manual);
    sc->addRegisterCallback(register_CCBuilderReader);
    sc->addRegisterCallback(jsb_register_chipmunk);
    sc->addRegisterCallback(jsb_register_system);
    sc->addRegisterCallback(JSB_register_opengl);
    sc->addRegisterCallback(MinXmlHttpRequest::_js_register);
    sc->addRegisterCallback(register_jsb_websocket);
    sc->addRegisterCallback(register_pc_bindings);
    sc->start();

    CCScriptEngineManager::sharedManager()->setScriptEngine(sc);
    sc->runScript((exeDir() + "\\shim.js").c_str());  // stands in for the Android-only native classes
    sc->runScript("main.js");                          // the game (UI/main.jsc)
    return true;
}

void AppDelegate::applicationDidEnterBackground()
{
    CCDirector::sharedDirector()->stopAnimation();
    SimpleAudioEngine::sharedEngine()->pauseBackgroundMusic();
    SimpleAudioEngine::sharedEngine()->pauseAllEffects();
}

void AppDelegate::applicationWillEnterForeground()
{
    CCDirector::sharedDirector()->startAnimation();
    SimpleAudioEngine::sharedEngine()->resumeBackgroundMusic();
    SimpleAudioEngine::sharedEngine()->resumeAllEffects();
}
