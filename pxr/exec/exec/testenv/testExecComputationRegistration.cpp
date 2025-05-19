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
#include "pxr/exec/esfUsd/sceneAdapter.h"

#include "pxr/base/arch/systemInfo.h"
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

#include <algorithm>
#include <iostream>
#include <iterator>
#include <set>

PXR_NAMESPACE_USING_DIRECTIVE;

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (attr)
    (attributeComputation)
    (attributeComputedValueComputation)
    (attributeName)
    (baseAndDerivedSchemaComputation)
    (derivedSchemaComputation)
    (emptyComputation)
    (missingComputation)
    (namespaceAncestorInput)
    (noInputsComputation)
    (primComputation)
    (stageAccessComputation)
    (unknownSchemaTypeComputation)
);

// A type that is not registered with TfType.
EXEC_REGISTER_COMPUTATIONS_FOR_SCHEMA(TestUnknownSchemaType)
{
    self.PrimComputation(_tokens->unknownSchemaTypeComputation)
        .Callback<double>(+[](const VdfContext &) { return 1.0; });
}

EXEC_REGISTER_COMPUTATIONS_FOR_SCHEMA(
    TestExecComputationRegistrationCustomSchema)
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

    self.PrimComputation(_tokens->baseAndDerivedSchemaComputation)
        .Callback(+[](const VdfContext &) { return 1.0; });
}

EXEC_REGISTER_COMPUTATIONS_FOR_SCHEMA(
    TestExecComputationRegistrationDerivedCustomSchema)
{
    self.PrimComputation(_tokens->derivedSchemaComputation)
        .Callback(+[](const VdfContext &) { return 1.0; });

    // This overrides the computation of the same name on the base schema.
    // (We add an input here so we can verify this definition is stronger.)
    self.PrimComputation(_tokens->baseAndDerivedSchemaComputation)
        .Callback(+[](const VdfContext &) { return 1.0; })
        .Inputs(
            AttributeValue<int>(_tokens->attributeName)
        );
}

// XXX:TODO
#if 0

// Test that clients can register schemas inside their own namespaces.

namespace client_namespace {

struct TestNamespacedSchemaType {};


EXEC_REGISTER_COMPUTATIONS_FOR_SCHEMA(TestNamespacedSchemaType)
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

// RAII class that verifies the expected number of errors is emitted during the
// lifetime of the object and that the commentary matches the expected error
// strings.
class ExpectedErrors {
public:
    ExpectedErrors(const std::set<std::string> &expectedErrors)
        : _expectedErrors(expectedErrors)
    {
    }

    ~ExpectedErrors() {
        const size_t numErrors = std::distance(_mark.begin(), _mark.end());
        ASSERT_EQ(numErrors, _expectedErrors.size());

        std::set<std::string> errors;
        for (auto it=_mark.begin(); it!=_mark.end(); ++it) {
            errors.insert(it->GetCommentary());
        }

        if (errors != _expectedErrors) {
            std::set<std::string> missingErrors, unexpectedErrors;
            std::set_difference(
                _expectedErrors.begin(), _expectedErrors.end(),
                errors.begin(), errors.end(),
                std::inserter(missingErrors, missingErrors.begin()));
            std::set_difference(
                errors.begin(), errors.end(),
                _expectedErrors.begin(), _expectedErrors.end(),
                std::inserter(unexpectedErrors, unexpectedErrors.begin()));

            std::string errorMessage =
                "Emitted errors differed from expected errors:\n";
            if (!missingErrors.empty()) {
                errorMessage += TfStringPrintf(
                    "Missing:\n  %s\n",
                    TfStringJoin(missingErrors, "\n  ").c_str());
            }
            if (!unexpectedErrors.empty()) {
                errorMessage += TfStringPrintf(
                    "Unexpected:\n  %s\n",
                    TfStringJoin(unexpectedErrors, "\n  ").c_str());
            }
            TF_FATAL_ERROR("%s", errorMessage.c_str());
        }

        _mark.Clear();
    }

private:
    const std::set<std::string> _expectedErrors;
    TfErrorMark _mark;
};

static EsfStage
_NewStageFromLayer(
    const char *const layerContents)
{
    const SdfLayerRefPtr layer = SdfLayer::CreateAnonymous(".usda");
    layer->ImportFromString(layerContents);
    TF_AXIOM(layer);
    const UsdStageRefPtr usdStage = UsdStage::Open(layer);
    TF_AXIOM(usdStage);
    return EsfUsdSceneAdapter::AdaptStage(usdStage);
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
    ExpectedErrors expected({
        "Attempt to register computation 'unknownSchemaTypeComputation' using "
        "an unknown type.",
        "Attempt to register computation '__computeTime' with a name that uses "
        "the prefix '__', which is reserved for builtin computations."
    });

    // The first time we pull on the defintion registry, errors for bad
    // registrations are emitted.
    Exec_DefinitionRegistry::GetInstance();
}

static void
TestUnknownSchemaType()
{
    EsfJournal *const nullJournal = nullptr;
    const Exec_DefinitionRegistry &reg = Exec_DefinitionRegistry::GetInstance();
    const EsfStage stage = _NewStageFromLayer(R"usd(#usda 1.0
        def TestUnknownSchemaType "Prim" {
        }
    )usd");
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
    TF_AXIOM(prim->IsValid(nullJournal));

    {
        ExpectedErrors expected({
            "Unknown schema type when looking up definition for computation "
            "'noInputsComputation'"
        });
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->noInputsComputation, nullJournal);
        TF_AXIOM(!primCompDef);
    }
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
        def TestUnknownSchemaType "Prim" {
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
    const EsfPrim pseudoroot = stage->GetPrimAtPath(SdfPath("/"), nullJournal);
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
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
            primCompDef->GetInputKeys(*prim, nullJournal)->Get().size(),
            0);
    }

    {
        // Look up a computation with no inputs.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->noInputsComputation, nullJournal);
        TF_AXIOM(primCompDef);

        ASSERT_EQ(
            primCompDef->GetInputKeys(*prim, nullJournal)->Get().size(),
            0);
    }

    {
        // Look up a stage bultin computation.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *pseudoroot, ExecBuiltinComputations->computeTime, nullJournal);
        TF_AXIOM(primCompDef);

        ASSERT_EQ(
            primCompDef->GetInputKeys(*prim, nullJournal)->Get().size(),
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
        ASSERT_EQ(inputKeys->Get().size(), 4);

        _PrintInputKeys(inputKeys->Get());

        size_t index = 0;
        {
            const Exec_InputKey &key = inputKeys->Get()[index++];
            ASSERT_EQ(key.inputName, _tokens->primComputation);
            ASSERT_EQ(key.computationName, _tokens->primComputation);
            ASSERT_EQ(key.resultType, TfType::Find<double>());
            ASSERT_EQ(key.providerResolution.localTraversal, SdfPath("."));
            ASSERT_EQ(key.providerResolution.dynamicTraversal,
                      ExecProviderResolution::DynamicTraversal::Local);
            ASSERT_EQ(key.optional, true);
        }

        {
            const Exec_InputKey &key = inputKeys->Get()[index++];
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
            const Exec_InputKey &key = inputKeys->Get()[index++];
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
            const Exec_InputKey &key = inputKeys->Get()[index++];
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
        ASSERT_EQ(inputKeys->Get().size(), 1);

        _PrintInputKeys(inputKeys->Get());

        const Exec_InputKey &key = inputKeys->Get()[0];
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
        ASSERT_EQ(inputKeys->Get().size(), 1);

        _PrintInputKeys(inputKeys->Get());

        const Exec_InputKey &key = inputKeys->Get()[0];
        ASSERT_EQ(key.inputName, ExecBuiltinComputations->computeValue);
        ASSERT_EQ(key.computationName, ExecBuiltinComputations->computeValue);
        ASSERT_EQ(key.resultType, TfType::Find<double>());
        ASSERT_EQ(key.providerResolution.localTraversal, SdfPath(".attr"));
        ASSERT_EQ(key.providerResolution.dynamicTraversal,
                  ExecProviderResolution::DynamicTraversal::Local);
        ASSERT_EQ(key.optional, true);
    }
}

static void
TestDerivedSchemaComputationRegistration()
{
    EsfJournal *const nullJournal = nullptr;
    const Exec_DefinitionRegistry &reg = Exec_DefinitionRegistry::GetInstance();
    const EsfStage stage = _NewStageFromLayer(R"usd(#usda 1.0
        def DerivedCustomSchema "Prim" {
        }
    )usd");
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
    TF_AXIOM(prim->IsValid(nullJournal));

    {
        // Look up a computation registered for the derived schema type.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->derivedSchemaComputation, nullJournal);
        TF_AXIOM(primCompDef);
    }

    {
        // Look up a computation registered for the base and derived schema
        // types.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->baseAndDerivedSchemaComputation, nullJournal);
        TF_AXIOM(primCompDef);

        // Make sure we got the definition from the derived schema (i.e., the
        // stronger one).
        const auto inputKeys =
            primCompDef->GetInputKeys(*prim, nullJournal);
        ASSERT_EQ(inputKeys->Get().size(), 1);
    }

    {
        // Look up a computation registered for the base schema type.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, _tokens->noInputsComputation, nullJournal);
        TF_AXIOM(primCompDef);
    }
}

static void
TestPluginSchemaComputationRegistration()
{
    EsfJournal *const nullJournal = nullptr;
    const Exec_DefinitionRegistry &reg = Exec_DefinitionRegistry::GetInstance();
    const EsfStage stage = _NewStageFromLayer(R"usd(#usda 1.0
        def PluginComputationSchema "Prim"
        {
        }

        def ExtraPluginComputationSchema "ExtraPrim"
        {
        }
    )usd");
    const EsfPrim prim = stage->GetPrimAtPath(SdfPath("/Prim"), nullJournal);
    TF_AXIOM(prim->IsValid(nullJournal));

    {
        ExpectedErrors expected({
            "Duplicate registrations of plugin computations for schema "
            "TestExecComputationRegistrationCustomSchema."
        });

        // Look up a computation registered in a plugin.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, TfToken("myComputation"), nullJournal);
        TF_AXIOM(primCompDef);

        const auto inputKeys =
            primCompDef->GetInputKeys(*prim, nullJournal);
        ASSERT_EQ(inputKeys->Get().size(), 2);
    }

    {
        // Look up another computation that was registered by the plugin we just
        // loaded.
        const Exec_ComputationDefinition *const primCompDef =
            reg.GetComputationDefinition(
                *prim, TfToken("anotherComputation"), nullJournal);
        TF_AXIOM(primCompDef);

        const auto inputKeys =
            primCompDef->GetInputKeys(*prim, nullJournal);
        ASSERT_EQ(inputKeys->Get().size(), 1);
    }

    {
        // Look up a computation on a prim with a different schema, which is
        // defined in the same plugin that defines computations for
        // PluginComputationSchema.
        const EsfPrim extraPrim =
            stage->GetPrimAtPath(SdfPath("/ExtraPrim"), nullJournal);
        TF_AXIOM(extraPrim->IsValid(nullJournal));

        const Exec_ComputationDefinition *const extraPrimCompDef =
            reg.GetComputationDefinition(
                *extraPrim, TfToken("myComputation"), nullJournal);
        TF_AXIOM(extraPrimCompDef);

        const auto inputKeys =
            extraPrimCompDef->GetInputKeys(*extraPrim, nullJournal);
        ASSERT_EQ(inputKeys->Get().size(), 0);
    }
}

static void
_SetupTestPlugins()
{
    const std::string pluginPath =
        TfStringCatPaths(
            TfGetPathName(ArchGetExecutablePath()),
            "ExecPlugins/lib/TestExecPluginComputation*/Resources/") + "/";

    const PlugPluginPtrVector plugins =
        PlugRegistry::GetInstance().RegisterPlugins(pluginPath);
    
    ASSERT_EQ(plugins.size(), 1);
    ASSERT_EQ(plugins[0]->GetName(), "TestExecPluginComputation");
}

int main()
{
    // Load the custom schema.
    const PlugPluginPtrVector testPlugins =
        PlugRegistry::GetInstance().RegisterPlugins(TfAbsPath("resources"));
    ASSERT_EQ(testPlugins.size(), 1);
    ASSERT_EQ(testPlugins[0]->GetName(), "testExecComputationRegistration");

    const TfType schemaType =
        TfType::FindByName("TestExecComputationRegistrationCustomSchema");
    TF_AXIOM(!schemaType.IsUnknown());

    _SetupTestPlugins();

    TestRegistrationErrors();
    TestUnknownSchemaType();
    TestStageBuiltinComputationOnPrim();
    TestComputationRegistration();
    TestDerivedSchemaComputationRegistration();
    TestPluginSchemaComputationRegistration();

    return 0;
}
