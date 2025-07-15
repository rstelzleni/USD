#include "pxr/pxr.h"
#include "pxr/usd/usdUtils/pythonLoggingDiagnosticDelegate.h"

#include "pxr/base/tf/pyResultConversions.h"

#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

#if PY_MAJOR_VERSION >= 3

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

// object == boost::python::object
object _GetLogger(UsdUtilsPythonLoggingDiagnosticDelegate &delegate)
{
    PyObject *logger = delegate.GetLogger();
    if (!logger) {
        return object();
    }
    // Borrow to increase the ref count
    return object(handle<>(borrowed(logger)));
}

void
wrapPythonLoggingDiagnosticDelegate()
{
    using This = UsdUtilsPythonLoggingDiagnosticDelegate;
    class_<This, noncopyable>(
        "PythonLoggingDiagnosticDelegate",
        "A diagnostic delegate that reports diagnostics through Python logging.",
        init<std::string>(
            (arg("loggerName") = std::string()),
            "Creates a new Python logging diagnostic delegate.\n"
            "A python logger will be created with the given name.\n"
            "If no name is provided the default is 'openusd'.\n\n"
            "USD will start logging to this logger as soon as the delegate is\ncreated. "
            "The logger will continue to be used until the delegate\nis destroyed."))
        .def("GetLogger", _GetLogger,
            "Get the Python logging.Logger object used by this delegate.")
        .def("ConfigureFormat", &This::ConfigureFormat,
            (arg("includeDiagnosticName") = false,
             arg("includeSourceInfo") = true,
             arg("includeSourceFunction") = false,
             arg("includeCommentary") = true),
            "Configure the format of diagnostic messages.\n\n"
            "Controls the information included in logged messages. The message\n"
            "is filled out in the same order as the parameters. By default it\n"
            "includes the source info and commentary.\n\n"
            "It is possible to configure all flags to false, in which case empty\n"
            "strings will be logged.\n\n"
            "Args:\n"
            "    includeDiagnosticName (bool): Include the diagnostic code\n"
            "    includeSourceInfo (bool): Include the source file and line number\n"
            "    includeSourceFunction (bool): Include the source function name\n"
            "    includeCommentary (bool): Include the commentary message")
        ;
}

#endif // PY_MAJOR_VERSION >= 3
