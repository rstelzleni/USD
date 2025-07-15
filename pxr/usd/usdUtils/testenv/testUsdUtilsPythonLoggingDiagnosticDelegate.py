#!/pxrpythonsubst

from pxr import Sdf, Tf, Usd, UsdGeom, UsdUtils

import logging
import unittest


class BufferedLogHandler(logging.Handler):
    """Helper for capturing log messages in a list."""
    def __init__(self):
        super().__init__()
        self.buffer = []

    def emit(self, record):
        msg = self.format(record)
        self.buffer.append(msg)

    def get_logs(self):
        return self.buffer

    def clear(self):
        self.buffer.clear()

    def print_logs(self):
        for msg in self.buffer:
            print("LOG MESSAGE", msg)
        self.clear()


def generate_warning():
    '''Named warning function to make intent clear.'''
    Sdf.Path('')


class TestUsdUtilsPythonLoggingDiagnosticDelegate(unittest.TestCase):
    def setUp(self):
        # Set up a logging handler to capture log messages
        self.handler = BufferedLogHandler()
        logging.getLogger().addHandler(self.handler)
        logging.getLogger().setLevel(logging.DEBUG)
        formatter = logging.Formatter('%(name)s - %(levelname)s - %(message)s')
        self.handler.setFormatter(formatter)

    def tearDown(self):
        logging.getLogger().removeHandler(self.handler)

    def testLoggingDiagnosticDelegate(self):
        # we haven't redirected output, so this will go to stderr
        generate_warning()

        # check that no messages were logged
        self.assertEqual(len(self.handler.get_logs()), 0)

        # Create a python logging delegate
        delegate = UsdUtils.PythonLoggingDiagnosticDelegate()

        # Generate some diagnostics
        generate_warning()

        # Check if log messages were captured
        self.assertGreater(len(self.handler.get_logs()), 0)

    def testErrors(self):
        delegate = UsdUtils.PythonLoggingDiagnosticDelegate()
        try:
            Tf.RaiseRuntimeError('Test error exception')
        except Tf.ErrorException as e:
            pass

        # This does not log an error, it's an exception
        self.assertEqual(len(self.handler.get_logs()), 0)

        # Raise an error from C++
        try:
            stage = Usd.Stage.CreateInMemory()
            UsdGeom.Xform.Define(stage, Sdf.Path('~~~'))
        except Tf.ErrorException as e:
            pass

        # Check that we got some logs. Note that this is just a warning
        # from Sdf, there is no error logged, because it was raised as an
        # exception.
        self.assertGreater(len(self.handler.get_logs()), 0)
        self.assertFalse(any('ERROR' in msg for msg in self.handler.get_logs()))

        # Now do some shenanigans to get an error logged
        self.handler.clear()
        try:
            stage = Usd.Stage.CreateInMemory()
            UsdGeom.Xform.Define(stage, Sdf.Path('~~~'))
        except Tf.ErrorException as e:
            Tf.RepostErrors(e)
        self.assertGreater(len(self.handler.get_logs()), 0)
        self.assertTrue(any('ERROR' in msg for msg in self.handler.get_logs()))

    def testWarnings(self):
        delegate = UsdUtils.PythonLoggingDiagnosticDelegate()
        generate_warning()

        # Check that we got some logs
        self.assertGreater(len(self.handler.get_logs()), 0)

        # Check that the warning message is in the logs
        log_messages = self.handler.get_logs()
        self.assertTrue(any('SdfPath' in msg for msg in log_messages))
        self.assertTrue(any('WARNING' in msg for msg in log_messages))
        self.handler.clear()

        # try again with a different warning
        Tf.Warn('This is a test warning')
        self.assertTrue(any('This is a test warning' in msg for msg in self.handler.get_logs()))

    def testInfoMessages(self):
        delegate = UsdUtils.PythonLoggingDiagnosticDelegate()
        Tf.Status('This is an info message')

        # Check that we got some logs
        self.assertGreater(len(self.handler.get_logs()), 0)

        # Check that the info message is in the logs
        log_messages = self.handler.get_logs()
        self.assertTrue(any('This is an info message' in msg for msg in log_messages))
        self.assertTrue(any('INFO' in msg for msg in log_messages))
        self.handler.clear()

    def testLoggingObject(self):
        delegate = UsdUtils.PythonLoggingDiagnosticDelegate()
        logger = delegate.GetLogger()
        self.assertIsInstance(logger, logging.Logger)
        self.assertEqual(logger.name, 'openusd')

    def testLoggerName(self):
        delegate = UsdUtils.PythonLoggingDiagnosticDelegate('pxr')
        logger = delegate.GetLogger()
        self.assertEqual(logger.name, 'pxr')
        Tf.Status('This is a message for pxr logger')
        self.assertEqual(len(self.handler.get_logs()), 1)
        logmessage = self.handler.get_logs()[0]
        self.assertTrue('pxr - INFO - ' in logmessage)
        self.assertTrue('This is a message for pxr logger' in logmessage)

    def testLoggerMessageFormatting(self):

        delegate = UsdUtils.PythonLoggingDiagnosticDelegate('usd tests')

        def testCombo(includeDiagnosticName, includeSourceInfo,
                      includeSourceFunction, includeCommentary):
            delegate.ConfigureFormat(
                includeDiagnosticName=includeDiagnosticName,
                includeSourceInfo=includeSourceInfo,
                includeSourceFunction=includeSourceFunction,
                includeCommentary=includeCommentary
            )
            Tf.Status('This is a message with combo features enabled')
            self.assertEqual(len(self.handler.get_logs()), 1)
            logmessage = self.handler.get_logs()[0]
            self.assertTrue('usd tests - INFO - ' in logmessage)
            if includeDiagnosticName:
                self.assertTrue('TF_DIAGNOSTIC_STATUS_TYPE' in logmessage)
            else:
                self.assertFalse('TF_DIAGNOSTIC_STATUS_TYPE' in logmessage)
            if includeSourceInfo:
                self.assertTrue('testUsdUtilsPythonLoggingDiagnosticDelegate' in logmessage)
            else:
                self.assertFalse('testUsdUtilsPythonLoggingDiagnosticDelegate' in logmessage)
            if includeSourceFunction:
                self.assertTrue('testCombo' in logmessage)
            else:
                self.assertFalse('testCombo' in logmessage)
            if includeCommentary:
                self.assertTrue('This is a message with combo features enabled' in logmessage)
            else:
                self.assertFalse('This is a message with combo features enabled' in logmessage)
            self.handler.clear()

        # Test all 16 combinations of the optional features
        #
        # Each combo is preceded by a comment indicating the expected log
        # message.
        #
        # Python logging will add more text depending on the configuration.
        # Could be logger name, time, etc. For this test the logger starts 
        # each log message with `usd tests - INFO - `. I have left this out.

        # TF_DIAGNOSTIC_STATUS_TYPE /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158: __main__.testCombo: This is a message with combo features enabled
        testCombo(True, True, True, True)

        # TF_DIAGNOSTIC_STATUS_TYPE /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158: __main__.testCombo:
        testCombo(True, True, True, False)

        # TF_DIAGNOSTIC_STATUS_TYPE /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158: This is a message with combo features enabled
        testCombo(True, True, False, True)

        # TF_DIAGNOSTIC_STATUS_TYPE /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158:
        testCombo(True, True, False, False)

        # TF_DIAGNOSTIC_STATUS_TYPE __main__.testCombo: This is a message with combo features enabled
        testCombo(True, False, True, True)

        # TF_DIAGNOSTIC_STATUS_TYPE __main__.testCombo:
        testCombo(True, False, True, False)

        # TF_DIAGNOSTIC_STATUS_TYPE This is a message with combo features enabled
        testCombo(True, False, False, True)

        # TF_DIAGNOSTIC_STATUS_TYPE
        testCombo(True, False, False, False)

        # /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158: __main__.testCombo: This is a message with combo features enabled
        testCombo(False, True, True, True)

        # /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158: __main__.testCombo:
        testCombo(False, True, True, False)

        # The Default
        # /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158: This is a message with combo features enabled
        testCombo(False, True, False, True)

        # /opt/USD/tests/testUsdUtilsPythonLoggingDiagnosticDelegate:158:
        testCombo(False, True, False, False)

        # __main__.testCombo: This is a message with combo features enabled
        testCombo(False, False, True, True)

        # __main__.testCombo:
        testCombo(False, False, True, False)

        # This is a message with combo features enabled
        testCombo(False, False, False, True)

        # 
        testCombo(False, False, False, False)

    def testMultipleDelegates(self):
        # Create two delegates
        delegate1 = UsdUtils.PythonLoggingDiagnosticDelegate('delegate1')
        delegate2 = UsdUtils.PythonLoggingDiagnosticDelegate('delegate2')
        delegate1.ConfigureFormat(
            includeDiagnosticName=False,
            includeSourceInfo=False,
            includeSourceFunction=False,
            includeCommentary=True
        )
        delegate2.ConfigureFormat(
            includeDiagnosticName=False,
            includeSourceInfo=False,
            includeSourceFunction=False,
            includeCommentary=True
        )

        # Generate a single diagnostic message
        Tf.Status('Funky Town')

        # Check that both delegates captured the log message
        self.assertEqual(len(self.handler.get_logs()), 2)
        log_messages = self.handler.get_logs()
        self.assertTrue(any('delegate1 - INFO - Funky Town' in msg for msg in log_messages))
        self.assertTrue(any('delegate2 - INFO - Funky Town' in msg for msg in log_messages))

    def testDelegatesWithSameName(self):
        # Create two delegates with the same name
        delegate1 = UsdUtils.PythonLoggingDiagnosticDelegate('duplicate')
        delegate2 = UsdUtils.PythonLoggingDiagnosticDelegate('duplicate')

        # Generate a single diagnostic message
        Tf.Status('Duplicate Delegates Test')

        # Check that both delegates captured the log message
        self.assertEqual(len(self.handler.get_logs()), 2)
        log_messages = self.handler.get_logs()
        self.assertEqual(log_messages[0], log_messages[1])


if __name__=="__main__":
    # Check if the PythonLoggingDiagnosticDelegate is available
    # It isn't built in python 2 builds
    if 'PythonLoggingDiagnosticDelegate' in dir(UsdUtils):
        unittest.main()

