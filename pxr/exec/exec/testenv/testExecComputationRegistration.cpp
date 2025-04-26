//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"

#include "pxr/exec/exec/builtinComputations.h"
#include "pxr/exec/exec/computationDefinition.h"
#include "pxr/exec/exec/definitionRegistry.h"
#include "pxr/exec/exec/registerSchema.h"

#include "pxr/exec/ef/time.h"
#include "pxr/exec/esf/stage.h"
#include "pxr/exec/execUsd/sceneAdapter.h"

#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/errorMark.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/smallVector.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/timeCode.h"

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE;

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (attr)
    (attributeComputation)
    (attributeComputedValueComputation)
    (attributeName)
    (emptyComputation)
    (missingComputation)
    (namespaceAncestorInput)
    (noInputsComputation)
    (primComputation)
    (stageAccessComputation)
);

// A type that is not registered with TfType.
EXEC_REGISTER_SCHEMA(TestUnknownType)
{
    self.PrimComputation(_tokens->noInputsComputation)
        .Callback<double>(+[](const VdfContext &) { return 1.0; });
}

EXEC_REGISTER_SCHEMA(TestExecComputationRegistrationCustomSchema)
{
    self.PrimComputation(_tokens->emptyComputation);

    // Attempt to register a prim computation that uses a builtin computation
    // name.
    self.PrimComputation(ExecBuiltinComputations->computeTime);

    self.PrimComputation(_tokens->noInputsComputation)
        .Callback(+[](const VdfContext &) { return 1.0; });

    // A prim computation that exercises various kinds of inputs.
    self.PrimComputation(_tokens->primComputation)
        .Callback<double>([](const VdfContext &ctx) { ctx.SetOutput(11.0); })
        .Inputs(
            Computation<double>(_tokens->primComputation),
            Attribute(_tokens->attributeName)
                .Computation<int>(_tokens->attributeComputation),
            AttributeValue<int>(_tokens->attributeName)
                .Required(),
            NamespaceAncestor<bool>(_tokens->primComputation)
                .InputName(_tokens->namespaceAncestorInput)
        );

    // A prim computation that returns the current time.
    self.PrimComputation(_tokens->stageAccessComputation)
        .Callback<EfTime>([](const VdfContext &ctx) {
            ctx.SetOutput(EfTime());
        })
        .Inputs(
            Stage()
                .Computation<EfTime>(ExecBuiltinComputations->computeTime)
                .Required()
        );

    // A prim computation that returns the value of the attribute 'attr' (of
    // type double), or 0.0, if there is no attribute of that name on the
    // owning prim.
    self.PrimComputation(_tokens->attributeComputedValueComputation)
        .Callback<double>([](const VdfContext &ctx) {
            const double *const valuePtr =
                ctx.GetInputValuePtr<double>(
                    ExecBuiltinComputations->computeValue);
            ctx.SetOutput(valuePtr ? *valuePtr : 0.0);
        })
        .Inputs(
            Attribute(_tokens->attr)
                .Computation<double>(ExecBuiltinComputations->computeValue)
        );
}

// XXX:TODO
#if 0

// Test that clients can register schemas inside their own namespaces.

namespace client_namespace {

struct TestNamespacedSchemaType {};


EXEC_REGISTER_SCHEMA(TestNamespacedSchemaType)
{
    self.PrimComputation(_tokens->noInputsComputation)
        .Callback(+[](const VdfContext &) { return 1.0; });
}

} // namespace client_namespace

#endif

#define ASSERT_EQ(expr, expected)                                       \
    [&] {                                                               \
        auto&& expr_ = expr;                                            \
        if (expr_ != expected) {                                        \
            TF_FATAL_ERROR(                                             \
                "Expected " TF_PP_STRINGIZE(expr) " == '%s'; got '%s'", \
                TfStringify(expected).c_str(),                          \
                TfStringify(expr_).c_str());                            \
        }                                                               \
     }()

static EsfStage
_NewStageFromLayer(
    const char *const layerContents)
{
    const SdfLayerRefPtr layer = SdfLayer::CreateAnonymous(".usda");
    layer->ImportFromString(layerContents);
    TF_AXIOM(layer);
    const UsdStageRefPtr usdStage = UsdStage::Open(layer);
    TF_AXIOM(usdStage);
    return ExecUsdSceneAdapter::AdaptStage(usdStage);
}

static void
_PrintInputKeys(
    const TfSmallVector<Exec_InputKey, 1> &inputKeys)
{
    std::cout << "\nPrinting " << inputKeys.size() << " input keys:\n";

    for (const Exec_InputKey &key : inputKeys) {
        std::cout << "\nkey:\n";
        std::cout << "  input name: " << key.inputName << "\n";
        std::cout << "  computation name: " << key.computationName << "\n";
        std::cout << "  result type: " << key.resultType << "\n";
        std::cout << "  local traversal path: "
                  << key.providerResolution.localTraversal << "\n";
        std::cout << "  traversal: "
                  << static_cast<int>(key.providerResolution.dynamicTraversal)
                  << "\n";
        std::cout << "  optional: " << key.optional << "\n";
    }
}

static void
TestRegistrationErrors()
{
    // The first time we pull on the defintion registry, errors for bad
    // registrations are emitted.
    TfErrorMark mark;
    std::cerr << "=== Expected Error Output Begin ===\n";
    Exec_DefinitionRegistry::GetInstance();
    std::cerr << "=== Expected Error Output End ===\n";

    static const std::vector expected{
        "Attempt to register computation 'noInputsComputation' using an unknown type.",
        "Attempt to register computation '__computeTime' with a name that uses the prefix '__', which is reserved for builtin computations."
    };

    size_t i=0;
    for (auto it=mark.begin(); it!=mark.end(); ++it) {
        ASSERT_EQ(it->GetCommentary(), expected[i++]);
    }
    ASSERT_EQ(i, 2);
}

static void
TestUnknownSchemaType()
{
    EsfJournal *const nullJournal = nullptr;
    const Exec_DefinitionRegistry &reg = Exec_DefinitionRegistry::GetInstance();
    const EsfStage stage = _NewStageFromLayer(R"usd(#usda 1.0
        def TestUnknownType "Prim" {
        }
    )usd");
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
    TF_AXIOM(prim->IsValid(nullJournal));

    const Exec_ComputationDefinition *const primCompDef =
        reg.GetComputationDefinition(
            *prim, _tokens->noInputsComputation, nullJournal);
    TF_AXIOM(!primCompDef);
}

// Test that attempts to look up builtin stage computations on prims (other
// than the pseudo-root) are rejected.
//
static void
TestStageBuiltinComputationOnPrim()
{
    EsfJournal *const nullJournal = nullptr;
    const Exec_DefinitionRegistry &reg = Exec_DefinitionRegistry::GetInstance();
    const EsfStage stage = _NewStageFromLayer(R"usd(#usda 1.0
        def TestUnknownType "Prim" {
        }
    )usd");
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
    TF_AXIOM(prim->IsValid(nullJournal));

    const Exec_ComputationDefinition *const primCompDef =
        reg.GetComputationDefinition(
            *prim, ExecBuiltinComputations->computeTime, nullJournal);
    TF_AXIOM(!primCompDef);
}

static void
TestComputationRegistration()
{
    EsfJournal *const nullJournal = nullptr;
    const Exec_DefinitionRegistry &reg = Exec_DefinitionRegistry::GetInstance();
    const EsfStage stage = _NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "Prim" {
        }
    )usd");
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
    const EsfPrim pseudoroot = stage->GetPrimAtPath(SdfPath("/"), nullJournal);
    TF_AXIOM(prim->IsValid(nullJournal));

    {
        // Look up a computation that wasn't registered.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->missingComputation, nullJournal);
        TF_AXIOM(!primCompDef);
    }

    {
        // Look up a computation with no callback or inputs.
        //
        // (Once we support composition of computation definitions, we will
        // want some kind of validation to ensure we end up with a callback.)
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->emptyComputation, nullJournal);
        TF_AXIOM(primCompDef);

        ASSERT_EQ(
            primCompDef->GetInputKeys(*prim, nullJournal).size(),
            0);
    }

    {
        // Look up a computation with no inputs.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->noInputsComputation, nullJournal);
        TF_AXIOM(primCompDef);

        ASSERT_EQ(
            primCompDef->GetInputKeys(*prim, nullJournal).size(),
            0);
    }

    {
        // Look up a stage bultin computation.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *pseudoroot, ExecBuiltinComputations->computeTime, nullJournal);
        TF_AXIOM(primCompDef);

        ASSERT_EQ(
            primCompDef->GetInputKeys(*prim, nullJournal).size(),
            0);
    }

    {
        // Look up a plugin computation on the stage pseudo-root.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *pseudoroot, _tokens->noInputsComputation, nullJournal);
        TF_AXIOM(!primCompDef);
    }

    {
        // Look up a computation with multiple inputs.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->primComputation, nullJournal);
        TF_AXIOM(primCompDef);

        const auto inputKeys =
            primCompDef->GetInputKeys(*prim, nullJournal);
        ASSERT_EQ(inputKeys.size(), 4);

        _PrintInputKeys(inputKeys);

        size_t index = 0;
        {
            const Exec_InputKey &key = inputKeys[index++];
            ASSERT_EQ(key.inputName, _tokens->primComputation);
            ASSERT_EQ(key.computationName, _tokens->primComputation);
            ASSERT_EQ(key.resultType, TfType::Find<double>());
            ASSERT_EQ(key.providerResolution.localTraversal, SdfPath("."));
            ASSERT_EQ(key.providerResolution.dynamicTraversal,
                      ExecProviderResolution::DynamicTraversal::Local);
            ASSERT_EQ(key.optional, true);
        }

        {
            const Exec_InputKey &key = inputKeys[index++];
            ASSERT_EQ(key.inputName, _tokens->attributeComputation);
            ASSERT_EQ(key.computationName, _tokens->attributeComputation);
            ASSERT_EQ(key.resultType, TfType::Find<int>());
            ASSERT_EQ(key.providerResolution.localTraversal,
                      SdfPath(".attributeName"));
            ASSERT_EQ(key.providerResolution.dynamicTraversal,
                      ExecProviderResolution::DynamicTraversal::Local);
            ASSERT_EQ(key.optional, true);
        }

        {
            const Exec_InputKey &key = inputKeys[index++];
            ASSERT_EQ(key.inputName, _tokens->attributeName);
            ASSERT_EQ(
                key.computationName, ExecBuiltinComputations->computeValue);
            ASSERT_EQ(key.resultType, TfType::Find<int>());
            ASSERT_EQ(key.providerResolution.localTraversal,
                      SdfPath(".attributeName"));
            ASSERT_EQ(key.providerResolution.dynamicTraversal,
                      ExecProviderResolution::DynamicTraversal::Local);
            ASSERT_EQ(key.optional, false);
        }

        {
            const Exec_InputKey &key = inputKeys[index++];
            ASSERT_EQ(key.inputName, _tokens->namespaceAncestorInput);
            ASSERT_EQ(key.computationName, _tokens->primComputation);
            ASSERT_EQ(key.resultType, TfType::Find<bool>());
            ASSERT_EQ(key.providerResolution.localTraversal, SdfPath("."));
            ASSERT_EQ(key.providerResolution.dynamicTraversal,
                      ExecProviderResolution::DynamicTraversal::
                          NamespaceAncestor);
            ASSERT_EQ(key.optional, true);
        }
    }

    {
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->stageAccessComputation, nullJournal);
        TF_AXIOM(primCompDef);

        const auto inputKeys =
            primCompDef->GetInputKeys(*prim, nullJournal);
        ASSERT_EQ(inputKeys.size(), 1);

        _PrintInputKeys(inputKeys);

        const Exec_InputKey &key = inputKeys[0];
        ASSERT_EQ(key.inputName, ExecBuiltinComputations->computeTime);
        ASSERT_EQ(key.computationName, ExecBuiltinComputations->computeTime);
        ASSERT_EQ(key.resultType, TfType::Find<EfTime>());
        ASSERT_EQ(key.providerResolution.localTraversal, SdfPath("/"));
        ASSERT_EQ(key.providerResolution.dynamicTraversal,
                  ExecProviderResolution::DynamicTraversal::Local);
        ASSERT_EQ(key.optional, false);
    }

    {
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->attributeComputedValueComputation,
                nullJournal);
        TF_AXIOM(primCompDef);

        const auto inputKeys =
            primCompDef->GetInputKeys(*prim, nullJournal);
        ASSERT_EQ(inputKeys.size(), 1);

        _PrintInputKeys(inputKeys);

        const Exec_InputKey &key = inputKeys[0];
        ASSERT_EQ(key.inputName, ExecBuiltinComputations->computeValue);
        ASSERT_EQ(key.computationName, ExecBuiltinComputations->computeValue);
        ASSERT_EQ(key.resultType, TfType::Find<double>());
        ASSERT_EQ(key.providerResolution.localTraversal, SdfPath(".attr"));
        ASSERT_EQ(key.providerResolution.dynamicTraversal,
                  ExecProviderResolution::DynamicTraversal::Local);
        ASSERT_EQ(key.optional, true);
    }
}

int main()
{
    // Load the custom schema.
    const PlugPluginPtrVector testPlugins =
        PlugRegistry::GetInstance().RegisterPlugins(TfAbsPath("resources"));
    TF_AXIOM(testPlugins.size() == 1);
    TF_AXIOM(testPlugins[0]->GetName() == "testExecComputationRegistration");

    const TfType schemaType =
        TfType::FindByName("TestExecComputationRegistrationCustomSchema");
    TF_AXIOM(!schemaType.IsUnknown());

    TestRegistrationErrors();
    TestUnknownSchemaType();
    TestStageBuiltinComputationOnPrim();
    TestComputationRegistration();

    return 0;
}
