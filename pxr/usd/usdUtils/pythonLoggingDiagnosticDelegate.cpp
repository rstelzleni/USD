#include "pxr/pxr.h"

#include "pxr/usd/usdUtils/pythonLoggingDiagnosticDelegate.h"

#include "pxr/base/arch/debugger.h"
#include "pxr/base/tf/pyLock.h"
#include "pxr/base/tf/pyUtils.h"

#if PY_MAJOR_VERSION >= 3

PXR_NAMESPACE_OPEN_SCOPE

UsdUtilsPythonLoggingDiagnosticDelegate::UsdUtilsPythonLoggingDiagnosticDelegate(
        const std::string &loggerName)
{
    if (!TfPyIsInitialized()) {
        TF_RUNTIME_ERROR("Can't setup logging, Python is not initialized.");
        return;
    }

    // Get the python logging module
    TfPyLock pyLock;
    PyObject *loggingModule = PyImport_ImportModule("logging");
    if (!loggingModule) {
        TF_RUNTIME_ERROR("Failed to import python logging module");
        TfPyPrintError();
        return;
    }

    // Get the logger creation function
    PyObject *getLogger = PyObject_GetAttrString(loggingModule, "getLogger");
    if (!getLogger || !PyCallable_Check(getLogger)) {
        Py_DECREF(loggingModule);
        if (getLogger) {
            Py_DECREF(getLogger);
        }
        TF_RUNTIME_ERROR("Failed to get 'getLogger' from logging module");
        TfPyPrintError();
        return;
    }
    
    // Create a logger name
    PyObject *loggerNameObj = PyUnicode_FromString(
            loggerName.empty() ? "openusd" : loggerName.c_str());
    if (!loggerNameObj) {
        Py_DECREF(getLogger);
        Py_DECREF(loggingModule);
        TF_RUNTIME_ERROR("Failed to create logger name string");
        TfPyPrintError();
        return;
    }

    // Call getLogger to create the logger
    _pyLogger = PyObject_CallFunctionObjArgs(getLogger, loggerNameObj, NULL);
    Py_DECREF(loggerNameObj);
    Py_DECREF(getLogger);
    Py_DECREF(loggingModule);
    if (!_pyLogger) {
        TF_RUNTIME_ERROR("Failed to create logger object");
        TfPyPrintError();
        return;
    }

    // Create python strings for logging method names
    _info = PyUnicode_FromString("info");
    _error = PyUnicode_FromString("error");
    _warning = PyUnicode_FromString("warning");
    _critical = PyUnicode_FromString("critical");
    if (!_info || !_error || !_warning || !_critical) {
        Py_DECREF(_pyLogger);
        Py_XDECREF(_info);
        Py_XDECREF(_warning);
        Py_XDECREF(_error);
        Py_XDECREF(_critical);
        _pyLogger = NULL;
        TF_RUNTIME_ERROR("Failed to create logging method strings");
        TfPyPrintError();
        return;
    }

    TfDiagnosticMgr::GetInstance().AddDelegate(this);
}

UsdUtilsPythonLoggingDiagnosticDelegate::~UsdUtilsPythonLoggingDiagnosticDelegate()
{
    TfDiagnosticMgr::GetInstance().RemoveDelegate(this);

    if (_pyLogger) {
        Py_DECREF(_pyLogger);
        Py_DECREF(_info);
        Py_DECREF(_warning);
        Py_DECREF(_error);
        Py_DECREF(_critical);
    }
}

void 
UsdUtilsPythonLoggingDiagnosticDelegate::IssueStatus(const TfStatus &status)
{
    if (!_pyLogger) {
        return;
    }

    std::string formattedMessage;
    _FormatMessage(status, &formattedMessage);

    TfPyLock pyLock;
    PyObject *pyMessage = PyUnicode_FromString(formattedMessage.c_str());
    if (!pyMessage) {
        TfPyPrintError();
        return;
    }

    PyObject *result = PyObject_CallMethodObjArgs(
            _pyLogger, _info, pyMessage, NULL);
    Py_DECREF(pyMessage);
    if (!result) {
        TfPyPrintError();
    } else {
        Py_DECREF(result);
    }
}

void
UsdUtilsPythonLoggingDiagnosticDelegate::IssueWarning(const TfWarning &warn)
{
    if (!_pyLogger) {
        return;
    }

    std::string formattedMessage;
    _FormatMessage(warn, &formattedMessage);

    TfPyLock pyLock;
    PyObject *pyMessage = PyUnicode_FromString(formattedMessage.c_str());
    if (!pyMessage) {
        TfPyPrintError();
        return;
    }

    PyObject *result = PyObject_CallMethodObjArgs(
            _pyLogger, _warning, pyMessage, NULL);
    Py_DECREF(pyMessage);
    if (!result) {
        TfPyPrintError();
    } else {
        Py_DECREF(result);
    }
}

void
UsdUtilsPythonLoggingDiagnosticDelegate::IssueError(const TfError &err)
{
    if (!_pyLogger) {
        return;
    }

    std::string formattedMessage;
    _FormatMessage(err, &formattedMessage);

    TfPyLock pyLock;
    PyObject *pyMessage = PyUnicode_FromString(formattedMessage.c_str());
    if (!pyMessage) {
        TfPyPrintError();
        return;
    }

    PyObject *result = PyObject_CallMethodObjArgs(
            _pyLogger, _error, pyMessage, NULL);
    Py_DECREF(pyMessage);
    if (!result) {
        TfPyPrintError();
    } else {
        Py_DECREF(result);
    }
}

void
UsdUtilsPythonLoggingDiagnosticDelegate::IssueFatalError(
    const TfCallContext &context, const std::string &message)
{
    // When running in a python environment I'm not sure if we can
    // trigger this code. The fatal error will be captured and handled
    // before going to this delegate.
    //
    // Still providing an implementation since the interface requires it.
    if (!_pyLogger) {
        return;
    }

    TfPyLock pyLock;
    PyObject *pyMessage = PyUnicode_FromString(message.c_str());
    if (!pyMessage) {
        TfPyPrintError();
        return;
    }

    // tf/pyTracing.h offers some support for stacks in python C apis,
    // we may be able to leverage that to convert the TfCallContext
    // into extra info that we could provide in kwargs. We could also
    // generate a stack to include in kwargs. Since I don't know of any
    // way to trigger this code I'm not putting in that work.
    //
    // If we do get here we will _not_ log a stack trace.
    PyObject *result = PyObject_CallMethodObjArgs(
            _pyLogger, _critical, pyMessage, NULL);
    Py_DECREF(pyMessage);
    if (!result) {
        TfPyPrintError();
    } else {
        Py_DECREF(result);
    }

    // Based on the implementation in CoalescingDiagnosticDelegate, I
    // think the delegate is responsible for aborting.
    ArchAbort(/*logging=*/ false);
}

void
UsdUtilsPythonLoggingDiagnosticDelegate::ConfigureFormat(
    bool includeDiagnosticName,
    bool includeSourceInfo,
    bool includeSourceFunction,
    bool includeCommentary)
{
    _includeDiagnosticName = includeDiagnosticName;
    _includeSourceInfo = includeSourceInfo;
    _includeSourceFunction = includeSourceFunction;
    _includeCommentary = includeCommentary;
}

void
UsdUtilsPythonLoggingDiagnosticDelegate::_FormatMessage(
    const TfDiagnosticBase &diagnostic,
    std::string *message) const
{
    if (!message) {
        return;
    }

    // Some best intention estimates for common sizes
    size_t estimate = _includeDiagnosticName ? 32 : 0 +
                      _includeSourceInfo ? 128 : 0 +
                      _includeSourceFunction ? 32 : 0 +
                      _includeCommentary ? 128 : 0;
    message->reserve(estimate);

    if (_includeDiagnosticName) {
        message->append(diagnostic.GetDiagnosticCodeAsString());
        message->append(" ");
    }

    if (_includeSourceInfo) {
        message->append(diagnostic.GetSourceFileName());
        message->append(":");
        message->append(std::to_string(diagnostic.GetSourceLineNumber()));
        message->append(": ");
    }

    if (_includeSourceFunction) {
        message->append(diagnostic.GetSourceFunction());
        message->append(": ");
    }

    if (_includeCommentary) {
        message->append(diagnostic.GetCommentary());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PY_MAJOR_VERSION >= 3

