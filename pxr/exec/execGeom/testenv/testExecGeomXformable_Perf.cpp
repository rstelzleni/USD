//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"

#include "pxr/exec/execGeom/tokens.h"

#include "pxr/exec/execUsd/request.h"
#include "pxr/exec/execUsd/system.h"
#include "pxr/exec/execUsd/valueKey.h"

#include "pxr/base/arch/timing.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/tf/pxrCLI11/CLI11.h"
#include "pxr/base/trace/aggregateNode.h"
#include "pxr/base/trace/collector.h"
#include "pxr/base/trace/reporter.h"
#include "pxr/base/trace/trace.h"
#include "pxr/base/work/threadLimits.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/stage.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace pxr_CLI;

// Creates a hierarchy of Xform prims.
static void
_CreateDescendantPrims(
    const SdfPrimSpecHandle root,
    const unsigned branchingFactor,
    const unsigned treeDepth,
    std::vector<SdfPath> *const leafPrims)
{
    // Traversal state vector: Each entry contains the parent prim spec and
    // the current traversal depth.
    std::vector<std::pair<SdfPrimSpecHandle, unsigned>>
        traversalState{{root, 1}};

    while (!traversalState.empty()) {
        auto [parent, currentDepth] = traversalState.back();
        traversalState.pop_back();

        ++currentDepth;
        if (currentDepth > treeDepth) {
            continue;
        }

        for (unsigned i=0; i<branchingFactor; ++i) {
            const SdfPrimSpecHandle primSpec =
                SdfPrimSpec::New(
                    parent,
                    TfStringPrintf("Prim%u", i),
                    SdfSpecifierDef, "Xform");
            TF_AXIOM(primSpec);

            const VtValue xformOpValue(VtStringArray({"xformOp:transform"}));
            const SdfAttributeSpecHandle xformOpAttr =
                SdfAttributeSpec::New(
                    primSpec,
                    "xformOpOrder",
                    SdfGetValueTypeNameForValue(xformOpValue),
                    SdfVariabilityUniform);
            xformOpAttr->SetDefaultValue(xformOpValue);

            GfMatrix4d transform{1};
            transform.SetTranslate(GfVec3d(1, 0, 0));
            const VtValue transformValue{transform};
            const SdfAttributeSpecHandle transformAttr =
                SdfAttributeSpec::New(
                    primSpec,
                    "xformOps:transform",
                    SdfGetValueTypeNameForValue(transformValue));
            transformAttr->SetDefaultValue(transformValue);

            if (currentDepth == treeDepth) {
                leafPrims->push_back(primSpec->GetPath());
            }

            traversalState.emplace_back(primSpec, currentDepth);
        }
    }
}

// Creates a stage and populates it with a hierarchy of Xform prims with the
// given branching factor and depth.
static UsdStageConstRefPtr
_CreateStage(
    const unsigned branchingFactor,
    const unsigned treeDepth,
    std::vector<SdfPath> *const leafPrims)
{
    TRACE_FUNCTION();

    std::cout << "Creating Xform tree with branching factor "
              << branchingFactor << " and tree depth "
              << treeDepth << "\n";

    const unsigned long numPrims =
        branchingFactor == 1
        ? treeDepth
        : (std::pow(branchingFactor, treeDepth) - 1) / (branchingFactor - 1);
    const unsigned long numLeafPrims = std::pow(branchingFactor, treeDepth-1);
    std::cout << "The tree will contain " << numPrims
              << " prims and " << numLeafPrims << " leaf prims.\n";

    const SdfLayerRefPtr layer = SdfLayer::CreateAnonymous(".usda");

    const SdfPrimSpecHandle primSpec =
        SdfPrimSpec::New(layer, "Root", SdfSpecifierDef, "Xform");
    TF_AXIOM(primSpec);

    const VtValue xformOpValue(VtStringArray({"xformOp:transform"}));
    const SdfAttributeSpecHandle xformOpAttr =
        SdfAttributeSpec::New(
            primSpec,
            "xformOpOrder",
            SdfGetValueTypeNameForValue(xformOpValue),
            SdfVariabilityUniform);
    xformOpAttr->SetDefaultValue(xformOpValue);

    const VtValue transformValue(GfMatrix4d(1));
    const SdfAttributeSpecHandle transformAttr =
        SdfAttributeSpec::New(
            primSpec,
            "xformOps:transform",
            SdfGetValueTypeNameForValue(transformValue));
    transformAttr->SetDefaultValue(transformValue);

    _CreateDescendantPrims(
        primSpec, branchingFactor, treeDepth, leafPrims);

    // Make sure we ended up with the correct number of leaf nodes.
    TF_AXIOM(leafPrims->size() == numLeafPrims);

    const UsdStageConstRefPtr usdStage = UsdStage::Open(layer);
    TF_AXIOM(usdStage);

    return usdStage;
}

// Looks for the trace aggregate node with the given key among the children of
// the given parent node.
//
static TraceAggregateNodePtr
_FindTraceNode(
    const TraceAggregateNodePtr &parent,
    const std::string &key)
{
    for (const TraceAggregateNodePtr &child : parent->GetChildren()) {
        // We look for a key that ends with the search string, rather than
        // require an exact match to account for the fact that in pxr-namespaced
        // builds, trace function keys are generated from namespaced symbols.
        if (TfStringEndsWith(child->GetKey().GetString(), key)) {
            return child;
        }
    }

    return {};
}

static void
_WritePerfstats(const TraceReporterPtr &globalReporter)
{
    const TraceAggregateNodePtr root =
        globalReporter->GetAggregateTreeRoot();

    const TraceAggregateNodePtr mainThreadNode =
        _FindTraceNode(root, "Main Thread");
    TF_AXIOM(mainThreadNode);
    const double mainThreadTime =
        ArchTicksToSeconds(
            uint64_t(mainThreadNode->GetInclusiveTime() * 1e3));

    const TraceAggregateNodePtr compileNode =
        _FindTraceNode(mainThreadNode, "ExecUsd_RequestImpl::Compile");
    TF_AXIOM(compileNode);
    const double compileTime =
        ArchTicksToSeconds(
            uint64_t(compileNode->GetInclusiveTime() * 1e3));

    const TraceAggregateNodePtr scheduleNode =
        _FindTraceNode(mainThreadNode, "VdfScheduler::Schedule");
    TF_AXIOM(scheduleNode);
    const double scheduleTime =
        ArchTicksToSeconds(
            uint64_t(scheduleNode->GetInclusiveTime() * 1e3));

    std::ofstream statsFile("perfstats.raw");
    static const char *const metricTemplate =
        "{'profile':'%s','metric':'time','value':%f,'samples':1}\n";
    statsFile << TfStringPrintf(metricTemplate, "time", mainThreadTime);
    statsFile << TfStringPrintf(metricTemplate, "compile_time", compileTime);
    statsFile << TfStringPrintf(metricTemplate, "schedule_time", scheduleTime);
}

static void
TestExecGeomXformable_Perf(
    unsigned branchingFactor,
    unsigned treeDepth,
    bool outputAsSpy)
{
    TraceCollector::GetInstance().SetEnabled(true);

    // Instantiate a hierarchy of Xform prims on a stage and get access to the
    // leaf prims.
    std::vector<SdfPath> leafPrims;
    const UsdStageConstRefPtr usdStage =
        _CreateStage(branchingFactor, treeDepth, &leafPrims);

    // Call IsValid on an attribute as a way to ensure that the
    // UsdSchemaRegistry has been populated before starting compilation.
    {
        TRACE_SCOPE("Preroll stage access");
        UsdPrim prim = usdStage->GetPrimAtPath(SdfPath("/Root"));
        UsdAttribute attribute =
            prim.GetAttribute(TfToken("xformOps:transform"));
        TF_AXIOM(attribute.IsValid());
    }

    ExecUsdSystem execSystem(usdStage);

    // Create value keys that compute the transforms for all leaf prims in the
    // namespace hierarchy.
    std::vector<ExecUsdValueKey> valueKeys;
    valueKeys.reserve(leafPrims.size());
    for (const SdfPath &path : leafPrims) {
        UsdPrim prim = usdStage->GetPrimAtPath(path);
        TF_AXIOM(prim.IsValid());
        valueKeys.emplace_back(prim, ExecGeomXformableTokens->computeTransform);
    }

    const ExecUsdRequest request = execSystem.BuildRequest(std::move(valueKeys));
    TF_AXIOM(request.IsValid());

    execSystem.PrepareRequest(request);
    TF_AXIOM(request.IsValid());

    // TODO: Compute and extract values.

    TraceCollector::GetInstance().SetEnabled(false);

    const TraceReporterPtr globalReporter = TraceReporter::GetGlobalReporter();
    globalReporter->UpdateTraceTrees();

    {
        if (outputAsSpy) {
            std::ofstream traceFile("test.spy");
            globalReporter->SerializeProcessedCollections(traceFile);
        } else {
            std::ofstream traceFile("test.trace");
            globalReporter->Report(traceFile);
        }
    }

    _WritePerfstats(globalReporter);
}

int 
main(int argc, char **argv) 
{
    unsigned numThreads = WorkGetConcurrencyLimit();
    unsigned branchingFactor = 0;
    unsigned treeDepth = 0;
    bool outputAsSpy = false;

    // Set up arguments and their defaults
    CLI::App app(
        "Creates a transform hierarchy by building a regular tree of Xform "
        "prims where each prim has <branchingFactor> children with an overall "
        "tree depth of <treeDepth>.",
        "testExecGeomXformable_Perf");
    app.add_option(
        "--branchingFactor", branchingFactor,
        "Branching factor used to build the Xform tree")
        ->required(true);
    app.add_option(
        "--treeDepth", treeDepth,
        "The depth of the Xform tree to build.")
        ->required(true);
    app.add_option(
        "--numThreads", numThreads, "Number of threads to use");
    app.add_flag(
        "--spy", outputAsSpy,
        "Report traces in .spy format.");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Running with " << numThreads << " threads.\n";
    WorkSetConcurrencyLimit(numThreads);

    TestExecGeomXformable_Perf(branchingFactor, treeDepth, outputAsSpy);
}
