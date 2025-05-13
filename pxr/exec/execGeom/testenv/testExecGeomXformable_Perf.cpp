//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"

#include "pxr/exec/execUsd/request.h"
#include "pxr/exec/execUsd/system.h"
#include "pxr/exec/execUsd/valueKey.h"

#include "pxr/base/arch/timing.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"
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

// Recursively creates a hierarchy of Xform prims.
static void
_CreatePrimsRecursive(
    const SdfPrimSpecHandle parent,
    const size_t branchingFactor,
    const size_t treeDepth,
    const size_t currentDepth,
    std::vector<SdfPath> *const leafPrims)
{
    if (currentDepth >= treeDepth) {
        return;
    }

    for (size_t i=0; i<branchingFactor; ++i) {
        const SdfPrimSpecHandle primSpec =
            SdfPrimSpec::New(
                parent, TfStringPrintf("Prim%zu", i), SdfSpecifierDef, "Xform");
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

        if (currentDepth+1 == treeDepth) {
            leafPrims->push_back(primSpec->GetPath());
        }

        _CreatePrimsRecursive(
            primSpec, branchingFactor, treeDepth, currentDepth+1, leafPrims);
    }
}

// Creates a stage and populates it with a hierarchy of Xform prims with the
// given branching factor and depth.
static UsdStageConstRefPtr
_CreateStage(
    const size_t branchingFactor,
    const size_t treeDepth,
    std::vector<SdfPath> *const leafPrims)
{
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

    _CreatePrimsRecursive(
        primSpec, branchingFactor, treeDepth, 1, leafPrims);

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
        // require an exact match to account for the fact that in the cmake
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
    size_t branchingFactor,
    size_t treeDepth)
{
    // Instantiate a hierarchy of Xform prims on a stage and get access to the
    // leaf prims.
    std::vector<SdfPath> leafPrims;
    const UsdStageConstRefPtr usdStage =
        _CreateStage(branchingFactor, treeDepth, &leafPrims);

    // Make sure we ended up with the correct number of leaf nodes.
    TF_AXIOM(leafPrims.size() == std::pow(branchingFactor, treeDepth-1));

    TraceCollector::GetInstance().SetEnabled(true);

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
        valueKeys.push_back(
            {usdStage->GetPrimAtPath(path), TfToken("computeTransform")});
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
        std::ofstream traceFile("test.spy");
        globalReporter->SerializeProcessedCollections(traceFile);
    }

    _WritePerfstats(globalReporter);
}

int 
main(int argc, char **argv) 
{
    if (argc < 3) {
        static const std::string usage = R"usage(
usage: testExecGeomXformable_Perf <branchingFactor> <treeDepth>

Creates a transform hierarchy by building a regular tree of Xform prims where
each prim has <branchingFactor> children with an overall tree depth of
<treeDepth>.

)usage";
        std::cerr << usage;
        return 1;
    }

    const int branchingFactor = std::atoi(argv[1]);
    const int treeDepth = std::atoi(argv[2]);

    std::cout << "Creating Xform tree with branching factor "
              << branchingFactor << " and tree depth "
              << treeDepth << "\n";

    // TODO: Support single-threaded versions of this test.
    WorkSetMaximumConcurrencyLimit();

    TestExecGeomXformable_Perf(branchingFactor, treeDepth);
}
