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

#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/exec/exec/computationBuilders.h"
#include "pxr/exec/exec/registerSchema.h"
#include "pxr/exec/exec/systemDiagnostics.h"
#include "pxr/exec/vdf/context.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/common.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"

PXR_NAMESPACE_USING_DIRECTIVE;

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    
    (computeUsingCustomAttr)
    (customAttr)
);

static void
ConfigureTestPlugin()
{
    const PlugPluginPtrVector testPlugins = PlugRegistry::GetInstance()
        .RegisterPlugins(TfAbsPath("resources"));

    TF_AXIOM(testPlugins.size() == 1);
    TF_AXIOM(testPlugins[0]->GetName() == "testExecUsdRecompilation");
}

static int
CommonComputationCallback(const VdfContext &ctx)
{
    return 42;
}

EXEC_REGISTER_COMPUTATIONS_FOR_SCHEMA(TestExecUsdRecompilationCustomSchema)
{
    // Computation depends on customAttr only.
    self.PrimComputation(_tokens->computeUsingCustomAttr)
        .Callback(CommonComputationCallback)
        .Inputs(
            AttributeValue<int>(_tokens->customAttr));
}

class Fixture
{
public:
    ExecUsdSystem &NewSystemFromLayer(const char *const layerContents) {
        TF_AXIOM(!_system);

        const SdfLayerRefPtr layer = SdfLayer::CreateAnonymous(".usda");
        layer->ImportFromString(layerContents);
        TF_AXIOM(layer);

        _stage = UsdStage::Open(layer);
        TF_AXIOM(_stage);
        _system.emplace(_stage);

        return *_system;
    }

    ExecUsdRequest BuildRequest(
        std::vector<ExecUsdValueKey> &&valueKeys) {
        return _system->BuildRequest(
            std::move(valueKeys));
    }

    UsdPrim GetPrimAtPath(const char *const pathStr) const {
        return _stage->GetPrimAtPath(SdfPath(pathStr));
    }

    UsdAttribute GetAttributeAtPath(const char *const pathStr) const {
        return _stage->GetAttributeAtPath(SdfPath(pathStr));
    }

    void GraphNetwork(const char *const filename) {
        ExecSystem::Diagnostics diagnostics(&*_system);
        diagnostics.GraphNetwork(filename);
    }

private:
    UsdStageRefPtr _stage;
    std::optional<ExecUsdSystem> _system;
};

static void
TestRecompileDisconnectedAttributeInput(Fixture &fixture)
{
    // Tests that we recompile a disconnected attribute input, when that
    // attribute comes into existence.

    ExecUsdSystem &system = fixture.NewSystemFromLayer(R"usd(#usda 1.0
        def CustomSchema "Prim" {
        }
    )usd");

    // Compile a leaf node and callback node for `computeUsingCustomAttr`.
    // The callback node's input for `customAttr` is disconnected because the
    // attribute does not exist.
    ExecUsdRequest request = fixture.BuildRequest({
        {fixture.GetPrimAtPath("/Prim"), _tokens->computeUsingCustomAttr}
    });
    system.PrepareRequest(request);
    fixture.GraphNetwork("TestRecompileDisconnectedAttributeInput-1.dot");

    // Create the attribute. The next round of compilation should compile and
    // connect the `customAttr` input of the callback node.
    fixture.GetPrimAtPath("/Prim").CreateAttribute(
        _tokens->customAttr, SdfValueTypeNames->Int);
    system.PrepareRequest(request);
    fixture.GraphNetwork("TestRecompileDisconnectedAttributeInput-2.dot");
}

static void
TestRecompileMultipleRequests(Fixture &fixture)
{
    // Tests that when we recompile a network, we recompile all inputs that
    // require recompilation, even those that do not contribute to the request
    // being compiled.

    ExecUsdSystem &system = fixture.NewSystemFromLayer(R"usd(#usda 1.0
        def CustomSchema "Prim1" {
            int customAttr = 10
        }
        def CustomSchema "Prim2" {
            int customAttr = 20
        }
    )usd");

    UsdPrim prim1 = fixture.GetPrimAtPath("/Prim1");
    UsdPrim prim2 = fixture.GetPrimAtPath("/Prim2");

    // Make 2 requests.
    ExecUsdRequest request1 = fixture.BuildRequest({
        {prim1, _tokens->computeUsingCustomAttr}
    });
    ExecUsdRequest request2 = fixture.BuildRequest({
        {prim2, _tokens->computeUsingCustomAttr}
    });
    
    // Compile the requests.
    system.PrepareRequest(request1);
    system.PrepareRequest(request2);
    fixture.GraphNetwork("TestRecompileMultipleRequests-1.dot");

    // Remove the custom attributes. This will uncompile both attribute input
    // nodes.
    prim1.RemoveProperty(_tokens->customAttr);
    prim2.RemoveProperty(_tokens->customAttr);
    fixture.GraphNetwork("TestRecompileMultipleRequests-2.dot");

    // Re-add both attributes.
    prim1.CreateAttribute(_tokens->customAttr, SdfValueTypeNames->Int);
    prim2.CreateAttribute(_tokens->customAttr, SdfValueTypeNames->Int);

    // By preparing just one of the requests, all inputs should be recompiled,
    // even those that only contribute to the other request.
    system.PrepareRequest(request1);
    fixture.GraphNetwork("TestRecompileMultipleRequests-3.dot");
}

int main()
{
    ConfigureTestPlugin();

    std::vector tests {
        TestRecompileDisconnectedAttributeInput,
        TestRecompileMultipleRequests
    };
    for (const auto &test : tests) {
        Fixture fixture;
        test(fixture);
    }
}
