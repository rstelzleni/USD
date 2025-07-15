
#ifndef USD_UTILS_PYTHON_LOGGING_DIAGNOSTIC_DELEGATE_H
#define USD_UTILS_PYTHON_LOGGING_DIAGNOSTIC_DELEGATE_H

#include "pxr/pxr.h"
#include "pxr/base/tf/pySafePython.h"
#include "pxr/base/tf/diagnosticMgr.h"
#include "pxr/usd/usdUtils/api.h"

#include <string>

// Don't even attempt to support python 2
#if PY_MAJOR_VERSION >= 3

PXR_NAMESPACE_OPEN_SCOPE

/// A class that reports USD diagnostics through standard python logging.
///
/// This does not create a new named logger for each pxr module. All USD
/// generated logs will fall under the same named logger, which defaults
/// to "openusd". Different names can be provided at construction time.
///
/// This diagnostic delegate registers itself on creation and is cleared when
/// destroyed.
///
/// If two of these are created at the same time and use the same logger name,
/// they will share the same python logger object. If they are created with
/// different names they will create different logger objects.
class UsdUtilsPythonLoggingDiagnosticDelegate : public TfDiagnosticMgr::Delegate {
public:
    /// Creates a new Python logging diagnostic delegate.
    ///
    /// A python logger will be created with the given name. If the name is an
    /// empty string, the default name "openusd" will be used.
    USDUTILS_API
    UsdUtilsPythonLoggingDiagnosticDelegate(
            const std::string &loggerName);

    USDUTILS_API
    virtual ~UsdUtilsPythonLoggingDiagnosticDelegate();

    // Methods that implement the interface provided in TfDiagnosticMgr::Delegate
    USDUTILS_API
    virtual void IssueError(const TfError &err) override;
    USDUTILS_API
    virtual void IssueWarning(const TfWarning &warn) override;
    USDUTILS_API
    virtual void IssueStatus(const TfStatus &status) override;
    USDUTILS_API
    virtual void IssueFatalError(const TfCallContext&, const std::string &) override;

    /// Returns the Python logger object used by this delegate.
    ///
    /// This allows python users to access the logger directly, for example
    /// to set the logging level or to add handlers.
    USDUTILS_API
    PyObject* GetLogger() const {
        return _pyLogger;
    }

    /// Configure the way diagnostics are formatted.
    ///
    /// Controls the information included in logged messages. The message
    /// is filled out in the same order as the parameters. By default it
    /// includes the source info and commentary.
    ///
    /// It is possible to configure all flags to false, in which case empty
    /// strings will be logged.
    ///
    /// \param includeDiagnosticName Include strings like TF_CODING_ERROR
    /// \param includeSourceInfo Include source file and line number
    /// \param includeSourceFunction Include source function name
    /// \param includeCommentary Include the commentary string
    USDUTILS_API
    void ConfigureFormat(
            bool includeDiagnosticName,
            bool includeSourceInfo,
            bool includeSourceFunction,
            bool includeCommentary);


private:

    void _FormatMessage(
            const TfDiagnosticBase &diagnostic,
            std::string *message) const;

    bool _includeDiagnosticName{false};
    bool _includeSourceInfo{true};
    bool _includeSourceFunction{false};
    bool _includeCommentary{true};

    PyObject *_pyLogger{NULL};
    PyObject * _info{NULL};
    PyObject * _warning{NULL};
    PyObject * _error{NULL};
    PyObject * _critical{NULL};
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PY_MAJOR_VERSION >= 3

#endif // USD_UTILS_PYTHON_LOGGING_DIAGNOSTIC_DELEGATE_H

