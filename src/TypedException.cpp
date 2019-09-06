#include "TypedException.h"

#include "Log.h"
#include "check.h"

TypedException apiVrapper2(const std::function<void()> &func) {
    try {
        func();
        return TypedException();
    } catch (const TypedException &e) {
        LOG2(e.file) << "Error " << std::to_string(e.numError) << ". " << e.description;
        return e;
    } catch (const Exception &e) {
        LOG2(e.file) << "Error " << e;
        return TypedException(TypeErrors::OTHER_ERROR, e);
    } catch (const std::exception &e) {
        LOG << "Error " << e.what();
        return TypedException(TypeErrors::OTHER_ERROR, e.what());
    } catch (...) {
        LOG << "Unknown error";
        return TypedException(TypeErrors::OTHER_ERROR, "Unknown error");
    }
}

TypedException::TypedException(const TypeErrors &numError, const Exception &exception)
    : numError(numError)
    , description(exception.message)
    , file(exception.file)
{}

TypedException apiVrapper2(const TypedException &exception, const std::function<void()> &func) {
    if (exception.isSet()) {
        return exception;
    }
    return apiVrapper2(func);
}
