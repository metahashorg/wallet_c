#include "FullWorker.h"

#include <functional>

#include "../TaskManager.h"

#include "../NsLookup.h"

#include "check.h"
#include "Log.h"

using namespace std::placeholders;

SET_LOG_NAMESPACE("NSL");

namespace nslookup {

static const std::string TYPE = "Full_Worker";
static const std::string SUB_TYPE = "Full";

static const seconds CONTROL_CHECK = 3min;
static const seconds CONTROL_CHECK_EXPIRE = CONTROL_CHECK + 2min;
static const seconds REPEAT_CHECK = hours(24);

FullWorker::FullWorker(TaskManager &manager, NsLookup &ns, const Task &task)
    : NslWorker(manager)
    , ns(ns)
{
    CHECK(task.name == TYPE, "Incorrect task on constructor");
}

bool FullWorker::isThisWorker(const std::string &taskName) {
    return taskName == TYPE;
}

std::string FullWorker::getType() const {
    return TYPE;
}

std::string FullWorker::getSubType() const {
    return SUB_TYPE;
}

Task FullWorker::makeTask(const seconds &remaining) {
    return Task(TYPE, QVariant(), remaining);
}

bool FullWorker::checkIsActual() const {
    const bool actual = checkSpentRecord(CONTROL_CHECK_EXPIRE);
    if (!actual) {
        LOG << "Full worker not actual";
    }
    return actual;
}

void FullWorker::runWorkImpl(WorkerGuard workerGuard) {
    beginWork(workerGuard);
}

void FullWorker::beginWork(const WorkerGuard &workerGuard) {
    tt.reset();
    LOG << "Full worker started";
    addNewTask(makeTask(CONTROL_CHECK));

    const auto finLookup = std::bind(&FullWorker::finalizeLookup, this, workerGuard);
    const auto beginPing = std::bind(&FullWorker::beginPing, this, workerGuard, _1);

    ns.beginResolve(allNodesForTypes, ipsTemp, finLookup, beginPing);
}

void FullWorker::beginPing(const WorkerGuard &workerGuard, std::map<QString, NodeType>::const_iterator node) {
    const auto continueResolve = std::bind(&FullWorker::continueResolve, this, workerGuard, node);

    ns.continuePing(std::begin(ipsTemp), node->second, allNodesForTypes, ipsTemp, continueResolve);
}

void FullWorker::continueResolve(const WorkerGuard &workerGuard, std::map<QString, NodeType>::const_iterator node) {
    const auto finalizeLookup = std::bind(&FullWorker::finalizeLookup, this, workerGuard);
    const auto beginPing = std::bind(&FullWorker::beginPing, this, workerGuard, _1);

    ns.continueResolve(std::next(node), allNodesForTypes, ipsTemp, finalizeLookup, beginPing);
}

void FullWorker::finalizeLookup(const WorkerGuard &workerGuard) {
    ns.finalizeLookup(true, allNodesForTypes);
    endWork(workerGuard);
}

void FullWorker::endWork(const WorkerGuard &workerGuard) {
    addSpentRecord();
    addNewTask(makeTask(REPEAT_CHECK));

    tt.stop();
    LOG << "Full worker finished. Time work: " << tt.countMs();

    finishWork(workerGuard);
}

} // namespace nslookup
