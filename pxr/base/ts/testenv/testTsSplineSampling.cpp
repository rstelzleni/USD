//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#include "pxr/pxr.h"
#include "pxr/base/ts/spline.h"
#include "pxr/base/ts/knot.h"
#include "pxr/base/gf/math.h"
#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/base/tf/enum.h"
#include "pxr/base/ts/tsTest_Museum.h"
#include "pxr/base/ts/tsTest_TsEvaluator.h"

#include <iostream>
#include <fstream>

PXR_NAMESPACE_USING_DIRECTIVE

static int testCase = 0;

static
void
DoOneSample(std::ostream& out,
            const TsSpline& spline,
            std::string sampleFunc,
            const GfInterval& timeInterval,
            double timeScale,
            double valueScale,
            double tolerance)
{
    ++testCase;
    
    // Sample the spline and output the results
    out << "Test Case " << testCase << ": "
        << sampleFunc << "("
        << timeInterval << ", "
        << timeScale << ", "
        << valueScale << ", "
        << tolerance << ")\n";

    bool result;
    if (sampleFunc == "Sample") {
        TsSplineSamples<GfVec2d> samples;
        result = spline.Sample(timeInterval,
                               timeScale,
                               valueScale,
                               tolerance,
                               &samples);

        if (!result) {
            out << "No result!\n";
        } else {
            for (size_t n = 0; n < samples.polylines.size(); ++n) {
                out << n << ": (source n/a)\n";
                for (const auto& vertex : samples.polylines[n]) {
                    out << "    " << vertex << "\n";
                }
            }
        }
    } else {
        TsSplineSamplesWithSources<GfVec2d> samples;
         
        result = spline.Sample(timeInterval,
                               timeScale,
                               valueScale,
                               tolerance,
                               &samples);
        if (!result) {
            out << "No result!\n";
        } else {
            for (size_t n = 0; n < samples.polylines.size(); ++n) {
                out << n
                    << ": ("
                    << TfEnum::GetName(samples.sources[n])
                    << ")\n";
                
                for (const auto& vertex : samples.polylines[n]) {
                    out << "    " << vertex << "\n";
                }
            }
        }
    }
    
    out << std::endl;
}

static
void DoTest(std::ostream& out, const std::string& sampleFunc)
{
    const std::vector<std::string> names = TsTest_Museum::GetAllNames();
    const TsTest_TsEvaluator evaluator;

    // Assume a 500x500 resolution.
    const int xPixels = 500, yPixels = 500;

    out << std::string(72, '#') << "\n"
        << "Testing " << sampleFunc << "\n"
        << std::string(72, '=')
        << std::endl;
    
    for (const std::string& name : names) {
        const TsTest_SplineData data = TsTest_Museum::GetDataByName(name);

        // Convert the generic spline data to an actual spline
        const TsSpline spline = evaluator.SplineDataToSpline(data);

        // Figure out the time and approximate value range of the spline
        const TsKnotMap knots = spline.GetKnots();
        GfInterval knotSpan = knots.GetTimeSpan();

        // Check for inner looping
        if (spline.HasInnerLoops()) {
            // The looped interval may or may not expand knotSpan
            knotSpan |= spline.GetInnerLoopParams().GetLoopedInterval();
        }
        double knotSpanSize = knotSpan.GetSize();

        // Calculate an extended time range that will include at least one
        // extrapolating loop pre and post (if there are extrapolating loops).
        GfInterval longSpan(knotSpan.GetMin() - 1.5 * knotSpanSize,
                            knotSpan.GetMax() + 1.5 * knotSpanSize);

        // Calculate a small span that is just the middle 50% of knotSpan
        GfInterval shortSpan(knotSpan.GetMin() + 0.25 * knotSpanSize,
                             knotSpan.GetMax() - 0.25 * knotSpanSize);

        // We would like to use spline.GetValueRange() but it is
        // "not yet implemented." Estimate by scanning through
        // the knot times and calling eval.  We're only using it
        // to compute reasonable scale factors.
        double minValue = std::numeric_limits<double>::infinity();
        double maxValue = -std::numeric_limits<double>::infinity();

        for (const TsKnot& knot : knots) {
            double value;
            if (spline.Eval(knot.GetTime(), &value)) {
                minValue = std::min(minValue, value);
                maxValue = std::max(maxValue, value);
            }
        }
        double valueRangeSize = maxValue - minValue;

        // Compute scales but don't divide by 0
        double timeScale = xPixels / std::max(knotSpanSize, 1.0);
        double valueScale = yPixels / std::max(valueRangeSize, 1.0);

        out << "Spline: " << name << "\n"
            << spline << "\n"
            << std::string(72, '-')
            << std::endl;

        // Sample the knots.
        DoOneSample(out, spline, sampleFunc,
                    knotSpan, timeScale, valueScale, 1.0);

        // Sample the extended range but with less rigor
        DoOneSample(out, spline, sampleFunc,
                    longSpan, timeScale, valueScale, 10.0);
        
        // Sample the short range but more rigor
        DoOneSample(out, spline, sampleFunc,
                    shortSpan, timeScale, valueScale, 0.5);
    }
}

void TestSample(std::ostream& out)
{
    DoTest(out, "Sample");
}

void TestSampleWithSources(std::ostream& out)
{
    DoTest(out, "SampleWithSources");
}

int main(int argc, const char **argv)
{
    std::ofstream outFile;
    std::ostream* outPtr = nullptr;
    const char* outName = "testTsSplineSampling.txt";
    if (argc > 1) {
        if (strcmp(argv[1], "-") == 0) {
            outPtr = &std::cout;
        } else {
            outName = argv[1];
        }
    }

    if (!outPtr) {
        outFile.open(outName);
        if (outFile.good()) {
            outPtr = &outFile;
        } else {
            std::cerr << "Error: Cannot open output file "
                      << '"' << argv[1] << '"'
                      << std::endl;
            return 1;
        }
    }

    TestSample(*outPtr);
    TestSampleWithSources(*outPtr);

    return 0;
}
