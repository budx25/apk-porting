#include "PcBindings.h"
#include "cocos2d.h"
#include "ScriptingCore.h"
#include "SafePath.h"
#include <windows.h>
#include <shellapi.h>
#include <string>

USING_NS_CC;

static bool s_debug = false;

void pc_set_debug(bool on) { s_debug = on; }

static std::wstring utf8_to_wide(const std::string& s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// Only web links: the game passes server-provided URLs, and anything else
// (file:, custom schemes) could start arbitrary programs.
static JSBool pc_openURL(JSContext* cx, uint32_t argc, jsval* vp)
{
    std::string url;
    bool ok = argc >= 1 && jsval_to_std_string(cx, JS_ARGV(cx, vp)[0], &url);
    if (ok && (url.compare(0, 7, "http://") == 0 || url.compare(0, 8, "https://") == 0))
    {
        HINSTANCE r = ShellExecuteW(NULL, L"open", utf8_to_wide(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
        ok = (INT_PTR)r > 32;
        CCLog("[pc] openURL %s", ok ? "opened" : "failed");
    }
    else
    {
        CCLog("[pc] openURL refused scheme '%s'", url.substr(0, url.find(':')).c_str());
        ok = false;
    }
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(ok));
    return JS_TRUE;
}

// Only files inside the game folder: game scripts arrive in server patches and can also
// send data out, so they must not be able to read anything else on the PC.
static JSBool pc_readTextFile(JSContext* cx, uint32_t argc, jsval* vp)
{
    std::string path;
    if (argc < 1 || !jsval_to_std_string(cx, JS_ARGV(cx, vp)[0], &path) || !isSafeRelativePath(path))
    {
        JS_SET_RVAL(cx, vp, JSVAL_NULL);
        return JS_TRUE;
    }
    unsigned long size = 0;
    CCFileUtils* fu = CCFileUtils::sharedFileUtils();
    unsigned char* data = fu->getFileData(fu->fullPathForFilename(path.c_str()).c_str(), "rb", &size);
    if (!data)
    {
        JS_SET_RVAL(cx, vp, JSVAL_NULL);
        return JS_TRUE;
    }
    std::string text((const char*)data, size);
    delete[] data;
    JS_SET_RVAL(cx, vp, std_string_to_jsval(cx, text));
    return JS_TRUE;
}

void register_pc_bindings(JSContext* cx, JSObject* global)
{
    JSObject* pc = JS_NewObject(cx, NULL, NULL, NULL);
    jsval pcVal = OBJECT_TO_JSVAL(pc);
    JS_SetProperty(cx, global, "pc", &pcVal);
    const unsigned flags = JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_ENUMERATE;
    JS_DefineFunction(cx, pc, "openURL", pc_openURL, 1, flags);
    JS_DefineFunction(cx, pc, "readTextFile", pc_readTextFile, 1, flags);
    jsval debug = BOOLEAN_TO_JSVAL(s_debug);
    JS_SetProperty(cx, pc, "debug", &debug);
}
