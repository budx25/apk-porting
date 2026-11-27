// Stands in for the native classes the Android build of Ninja Heroes provides (ts.*, px.*, anysdk,
// channelBinding). Loaded by the host before main.js. See the design doc's "Android shim" table.
// Logging rule: names and argument counts only, never values (they can hold account details).
(function (global) {
    var nativeLog = global.log;   // ScriptingCore's log(); cc.log only exists once main.js loads jsb.js
    function log(m) {
        var out = (typeof cc !== 'undefined' && cc.log) ? cc.log : nativeLog;
        out('[shim] ' + m);
    }
    function maskEmail(s) { return String(s).replace(/[^"_]+@[^"_]+/g, '<email>'); }

    var vf = {};
    try { vf = JSON.parse(pc.readTextFile('VERSION') || '{}'); } catch (e) { log('VERSION unreadable'); }

    // Traced in the Android engine (VersionUtil::VersionUtil, VersionScene::onSuccess): before main.js
    // runs it reads VERSION and saves "ch" under A5 (the script's StoreConst.sdk), and the client
    // version under A1, which the update screen rewrites after every patch. serverUtil reads both.
    // The launcher does the updating here, so A1 simply follows VERSION.
    function seedFromVersion(vf) {
        if (vf.ch) sys.localStorage.setItem('A5', vf.ch);
        if (vf.v) sys.localStorage.setItem('A1', vf.v);
    }
    seedFromVersion(vf);

    // Inert stand-in for anything not listed below: callable, constructible, chainable.
    function inert(path) {
        return new Proxy(function () {}, {
            get: function (t, key) {
                if (typeof key !== 'string') return undefined;
                if (key === 'toString' || key === 'valueOf') return function () { return ''; };
                return inert(path + '.' + key);
            },
            apply: function (t, self, args) { log(path + '(' + args.length + ' args)'); return inert(path + '()'); },
            construct: function () { log('new ' + path); return inert(path); }
        });
    }

    // Known members answer for real; anything else falls back to an inert stand-in.
    function withFallback(path, obj) {
        return new Proxy(obj, {
            get: function (t, key) { return (typeof key !== 'string' || key in t) ? t[key] : inert(path + '.' + key); }
        });
    }

    // A native class the game uses as `new X()`, `X.getInstance()` or with static calls (`X.method()`);
    // all three reach the same methods.
    function nativeClass(path, methods) {
        var instance = withFallback(path, methods);
        var cls = function () { return instance; };
        cls.getInstance = function () { return instance; };
        for (var k in methods) cls[k] = methods[k];
        return withFallback(path, cls);
    }

    global.ts = {
        CommonJsb: nativeClass('ts.CommonJsb', {
            getClientVersion: function () { return vf.v; },
            getTestVersion: function () { return false; },
            checkNetwork: function () { return true; },
            getAccountId: function () { return ''; },
            // Android returns Java NativeCall.getDeviceInfo(): a JSON object string. Its keys couldn't be
            // traced (shortcut: generic Windows values; trace NativeCall if the server ever rejects them).
            getClientDeviceInfo: function () { return JSON.stringify({ platform: 'windows', os: 'Windows', model: 'PC' }); }
        }),
        JSBTools: nativeClass('ts.JSBTools', {
            // The game shows its own "new version available" dialog first; updating is the launcher's job.
            goToVersion: function () { log('server wants a newer version: restart through the launcher'); }
        }),
        ChannelBinding: nativeClass('ts.ChannelBinding', {
            openURL: function (url) { return pc.openURL(String(url)); },
            loginGoogle: function () { log('Google sign-in is not available on PC'); },
            logoutGoogle: function () {},
            loginFacebook: function () { log('Facebook sign-in is not available on PC'); }
        })
    };

    // Saved data: kept in the engine's local storage. The Android build encrypts it, but it never
    // leaves this PC, so plain storage behaves the same for the game.
    function storageLog(op, key) { log('StorageUtil.' + op + '("' + maskEmail(key) + '")'); }
    global.px = {
        StorageUtil: nativeClass('px.StorageUtil', {
            getItem: function (k) { storageLog('getItem', k); return sys.localStorage.getItem(k) || null; },
            setItem: function (k, v) { storageLog('setItem', k); sys.localStorage.setItem(k, v); },
            removeItem: function (k) { storageLog('removeItem', k); sys.localStorage.removeItem(k); },
            encryt: function (s) { return s; },
            decrpt: function (s) { return s; }
        })
    };

    // Enter (sent by the host): press the button of the game's open prompt, but only when it has
    // exactly one. PromptView.currentPage is the open prompt; its buttons live in menuContainer.
    pc.onEnter = function () {
        var page = typeof PromptView !== 'undefined' ? PromptView.currentPage : null;
        if (!page || !page.isRunning() || !page.isVisible() || !(page.menuContainer instanceof cc.Menu)) return;
        var buttons = (page.menuContainer.getChildren() || []).filter(function (n) {
            return n instanceof cc.MenuItem && n.isVisible() && n.isEnabled();
        });
        if (buttons.length === 1) buttons[0].activate();
    };

    global.anysdk = inert('anysdk');
    global.channelBinding = new global.ts.ChannelBinding();

    if (pc.debug) {
        // Diagnostics for development only (started with --debug). Still no values in the log.
        var parse = JSON.parse;
        JSON.parse = function (s, reviver) {
            try { return parse.call(JSON, s, reviver); }
            catch (e) { log('JSON.parse failed on ' + typeof s + ' of length ' + String(s).length); throw e; }
        };
        var send = WebSocket.prototype.send;
        WebSocket.prototype.send = function (data) {
            log('ws> ' + String(data).length + ' chars');
            return send.apply(this, arguments);
        };
    }
    log('installed (version ' + vf.v + ', channel ' + vf.ch + ')');
})(this);
