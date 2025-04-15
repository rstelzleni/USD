#!/pxrpythonsubst

#
# Copyright 2025 Pixar
#
# Licensed under the terms set forth in the LICENSE.txt file available at
# https://openusd.org/license.
#

from pxr import Ts

import unittest

class TsTest_SplineValueBlocks(unittest.TestCase):
    def test_SplineValueBlocks(self):
        # Build a spline that looks like this:
        # [-inf .. 1.0): value block
        # [ 1.0 .. 2.0): linear from 1.0 to 2.0
        # [ 2.0 .. 3.0): value block
        # [ 3.0 .. 4.0): held from 3.0 to 4.0
        # [ 4.0 .. inf): value block
        spline = Ts.Spline()
        knots = [
            Ts.Knot(time=1.0, value=1.0, nextInterp=Ts.InterpLinear),
            Ts.Knot(time=2.0, value=2.0, nextInterp=Ts.InterpValueBlock),
            Ts.Knot(time=3.0, value=3.0, nextInterp=Ts.InterpHeld),
            Ts.Knot(time=4.0, value=4.0, nextInterp=Ts.InterpHeld),
        ]

        for knot in knots:
            spline.SetKnot(knot)

        extrapolation = Ts.Extrapolation(Ts.ExtrapValueBlock)
        spline.SetPreExtrapolation(extrapolation)
        spline.SetPostExtrapolation(extrapolation)

        # Check the preValue, value, preDerivative, and derivative.
        def Check(time, preValue, value, preDeriv, deriv, preHeld, held):
            if preValue is None:
                self.assertIsNone(spline.EvalPreValue(time))
            else:
                self.assertEqual(spline.EvalPreValue(time), preValue)

            if value is None:
                self.assertIsNone(spline.Eval(time))
            else:
                self.assertEqual(spline.Eval(time), value)

            if preDeriv is None:
                self.assertIsNone(spline.EvalPreDerivative(time))
            else:
                self.assertEqual(spline.EvalPreDerivative(time), preDeriv)

            if deriv is None:
                self.assertIsNone(spline.EvalDerivative(time))
            else:
                self.assertEqual(spline.EvalDerivative(time), deriv)

            if preHeld is None:
                self.assertIsNone(spline.EvalPreValueHeld(time))
            else:
                self.assertEqual(spline.EvalPreValueHeld(time), preHeld)

            if held is None:
                self.assertIsNone(spline.EvalHeld(time))
            else:
                self.assertEqual(spline.EvalHeld(time), held)

        #    time  pVal   Val   pDv   Dv   pHld   Hld
        Check(0.0, None, None, None, None, None, None)  # [-inf .. 1.0): block
        Check(1.0, None,  1.0, None,  1.0, None,  1.0)  # [ 1.0 .. 2.0): linear
        Check(1.5,  1.5,  1.5,  1.0,  1.0,  1.0,  1.0)
        Check(2.0,  2.0, None,  1.0, None,  1.0, None)  # [ 2.0 .. 3.0): block
        Check(2.5, None, None, None, None, None, None)
        Check(3.0, None,  3.0, None,  0.0, None,  3.0)  # [ 3.0 .. 4.0): held
        Check(3.5,  3.0,  3.0,  0.0,  0.0,  3.0,  3.0)
        Check(4.0,  3.0, None,  0.0, None,  3.0, None)  # [ 4.0 .. inf): block
        Check(5.0, None, None, None, None, None, None)

if __name__ == '__main__':
    unittest.main()
