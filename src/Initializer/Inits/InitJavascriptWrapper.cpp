#include "InitJavascriptWrapper.h"

#include "../Initializer.h"

#include <functional>
using namespace std::placeholders;

#include "JavascriptWrapper.h"
#include "mainwindow.h"

#include "Paths.h"
#include "check.h"
#include "TypedException.h"
#include "SlotWrapper.h"

namespace initializer {

QString InitJavascriptWrapper::stateName() {
    return "jsWrapper";
}

InitJavascriptWrapper::InitJavascriptWrapper(QThread *mainThread, Initializer &manager)
    : InitInterface(stateName(), mainThread, manager, false)
{
    CHECK(connect(this, &InitJavascriptWrapper::callbackCall, this, &InitJavascriptWrapper::onCallbackCall), "not connect onCallbackCall");
    qRegisterMetaType<Callback>("Callback");

    registerStateType("init", "jsWrapper initialized", true, true);
}

InitJavascriptWrapper::~InitJavascriptWrapper() = default;

void InitJavascriptWrapper::onCallbackCall(const Callback &callback) {
BEGIN_SLOT_WRAPPER
    callback();
END_SLOT_WRAPPER
}

void InitJavascriptWrapper::completeImpl() {
    CHECK(jsWrapper != nullptr, "jsWrapper not initialized");
}

void InitJavascriptWrapper::sendInitSuccess(const TypedException &exception) {
    sendState("init", false, exception);
}

InitJavascriptWrapper::Return InitJavascriptWrapper::initialize(
        std::shared_future<WebSocketClient*> wssClient,
        std::shared_future<NsLookup*> nsLookup,
        std::shared_future<MainWindow*> mainWindow,
        std::shared_future<std::pair<transactions::TransactionsJavascript*, transactions::Transactions*>> transactions,
        std::shared_future<std::pair<auth::Auth*, auth::AuthJavascript*>> auth,
        const QString &versionString
) {
    const TypedException exception = apiVrapper2([&, this] {
        MainWindow &mw = *mainWindow.get();
        jsWrapper = std::make_unique<JavascriptWrapper>(mw, *wssClient.get(), *nsLookup.get(), *transactions.get().second, *auth.get().first, versionString);
        jsWrapper->moveToThread(mainThread);
        emit mw.setJavascriptWrapper(jsWrapper.get(), std::bind(&InitJavascriptWrapper::callbackCall, this, _1), [this](const TypedException &e) {
            sendInitSuccess(e);
        });
    });

    if (exception.isSet()) {
        sendInitSuccess(exception);
        throw exception;
    }
    return jsWrapper.get();
}

}
