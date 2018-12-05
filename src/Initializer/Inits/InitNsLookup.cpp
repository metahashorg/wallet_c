#include "InitNsLookup.h"

#include "../Initializer.h"

#include "NsLookup.h"

#include "check.h"
#include "SlotWrapper.h"

namespace initializer {

InitNsLookup::InitNsLookup(QThread *mainThread, Initializer &manager)
    : QObject(nullptr)
    , InitInterface(mainThread, manager)
{}

InitNsLookup::~InitNsLookup() = default;

void InitNsLookup::complete() {
    CHECK(nsLookup != nullptr, "nsLookup not initialized");
}

void InitNsLookup::sendInitSuccess() {
    sendState(InitState("nslookup", "init", "nslookup initialized", TypedException()));
}

void InitNsLookup::sendFlushSuccess() {
    if (!isFlushed) {
        sendState(InitState("nslookup", "flushed", "nslookup flushed", TypedException()));
        isFlushed = true;
    }
}

void InitNsLookup::onServersFlushed() {
BEGIN_SLOT_WRAPPER
    sendFlushSuccess();
END_SLOT_WRAPPER
}

InitNsLookup::Return InitNsLookup::initialize() {
    nsLookup = std::make_unique<NsLookup>();
    CHECK(connect(nsLookup.get(), &NsLookup::serversFlushed, this, &InitNsLookup::onServersFlushed), "not connect onServersFlushed");
    if (nsLookup->isServFlushed()) { // Так как сигнал мог прийти до коннекта, проверим здесь
        sendFlushSuccess();
    }
    nsLookup->start();

    sendInitSuccess();
    return *nsLookup;
}

}
