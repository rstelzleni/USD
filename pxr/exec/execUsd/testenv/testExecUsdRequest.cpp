//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/execUsd/cacheView.h"
#include "pxr/exec/execUsd/request.h"
#include "pxr/exec/execUsd/system.h"
#include "pxr/exec/execUsd/valueKey.h"

#include "pxr/exec/exec/registerSchema.h"
#include "pxr/exec/vdf/context.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/work/loops.h"

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (computeXf)
    (xf)
    );

EXEC_REGISTER_SCHEMA(TestExecUsdRequestComputedTransform)
{
    self.PrimComputation(_tokens->computeXf)
        .Callback(+[](const VdfContext &ctx) {
            const GfMatrix4d id(1);
            const GfMatrix4d &xf = *ctx.GetInputValuePtr<GfMatrix4d>(
                _tokens->xf, &id);
            const GfMatrix4d &parentXf = *ctx.GetInputValuePtr<GfMatrix4d>(
                _tokens->computeXf, &id);
            return xf * parentXf;
        })
        .Inputs(
            // AttributeValue<GfMatrix4d>(_tokens->xf),
            NamespaceAncestor<GfMatrix4d>(_tokens->computeXf)
        );
}

static void
ConfigureTestPlugin()
{
    const PlugPluginPtrVector testPlugins = PlugRegistry::GetInstance()
        .RegisterPlugins(TfAbsPath("resources"));

    TF_AXIOM(testPlugins.size() == 1);
    TF_AXIOM(testPlugins[0]->GetName() == "testExecUsdRequest");
}

static UsdStageRefPtr
CreateTestStage()
{
    SdfLayerRefPtr layer = SdfLayer::CreateAnonymous(".usda");
    const bool importedLayer = layer->ImportFromString(
        R"usda(#usda 1.0
        (
            defaultPrim = "Root"
        )
        def ComputedTransform "Root" (
            kind = "component"
        )
        {
            def ComputedTransform "A1"
            {
                matrix4d xf = ( (2, 0, 0, 0), (0, 2, 0, 0), (0, 0, 2, 0), (0, 0, 0, 1) )
                def ComputedTransform "B"
                {
                    matrix4d xf = ( (3, 0, 0, 0), (0, 3, 0, 0), (0, 0, 3, 0), (0, 0, 0, 1) )
                }
            }
            def ComputedTransform "A2"
            {
                matrix4d xf = ( (5, 0, 0, 0), (0, 5, 0, 0), (0, 0, 5, 0), (0, 0, 0, 1) )
            }
        }
        )usda");
    TF_AXIOM(importedLayer);

    UsdStageRefPtr stage = UsdStage::Open(layer);
    TF_AXIOM(stage);
    return stage;
}

int
main(int argc, char* argv[])
{
    ConfigureTestPlugin();

    UsdStageRefPtr stage = CreateTestStage();

    ExecUsdSystem system(stage);

    ExecUsdRequest request = system.BuildRequest({
        {stage->GetPrimAtPath(SdfPath("/Root")), _tokens->computeXf},
        {stage->GetPrimAtPath(SdfPath("/Root/A1")), _tokens->computeXf},
        {stage->GetPrimAtPath(SdfPath("/Root/A1/B")), _tokens->computeXf},
        {stage->GetPrimAtPath(SdfPath("/Root/A2")), _tokens->computeXf},
    });
    TF_AXIOM(request.IsValid());

    system.PrepareRequest(request);
    TF_AXIOM(request.IsValid());

    ExecUsdCacheView view = system.CacheValues(request);

    // Value extraction is currently unimplemented so temporarily indicate
    // that error messages are expected here.
    fprintf(stderr, "--- BEGIN EXPECTED ERROR ---\n");
    // Extract values concurrently and repeatedly from the same index.
    WorkParallelForN(
        12345,
        [view](int i, int n) {
            for (; i!=n; ++i) {
                VtValue v;
                view.Extract(i%4, &v);
            }
        });
    fprintf(stderr, "--- END EXPECTED ERROR ---\n");

    return 0;
}
