//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"

#include "pxr/exec/exec/inputResolver.h"

#include "pxr/exec/exec/computationBuilders.h"
#include "pxr/exec/exec/computationDefinition.h"
#include "pxr/exec/exec/definitionRegistry.h"
#include "pxr/exec/exec/inputKey.h"
#include "pxr/exec/exec/outputKey.h"
#include "pxr/exec/exec/providerResolution.h"
#include "pxr/exec/exec/registerSchema.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/plug/notice.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/exec/esf/editReason.h"
#include "pxr/exec/esf/journal.h"
#include "pxr/exec/esf/object.h"
#include "pxr/exec/esf/stage.h"
#include "pxr/exec/execUsd/sceneAdapter.h"
#include "pxr/exec/vdf/context.h"
#include "pxr/usd/usd/common.h"
#include "pxr/usd/sdf/layer.h"

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE;

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (inputName)
    (customComputation)
    (nonExistentComputation)
);

#define ASSERT_EQ(expr, expected)                                              \
    [&] {                                                                      \
        auto&& expr_ = expr;                                                   \
        if (expr_ != expected) {                                               \
            TF_FATAL_ERROR(                                                    \
                "Expected " TF_PP_STRINGIZE(expr) " == '%s'; got '%s'",        \
                TfStringify(expected).c_str(),                                 \
                TfStringify(expr_).c_str());                                   \
        }                                                                      \
    }()

#define ASSERT_OUTPUT_KEY(actual, expectedProvider, expectedDefinition)        \
    {                                                                          \
        const Exec_OutputKey expected{expectedProvider, expectedDefinition};   \
        const Exec_OutputKey::Identity actualOutputKeyIdentity =               \
            (actual).MakeIdentity();                                           \
        const Exec_OutputKey::Identity expectedOutputKeyIdentity =             \
            expected.MakeIdentity();                                           \
        ASSERT_EQ(actualOutputKeyIdentity, expectedOutputKeyIdentity);         \
    }

PXR_NAMESPACE_OPEN_SCOPE

static std::ostream &
operator<<(std::ostream &out, const Exec_OutputKey::Identity &outputKeyIdentity)
{
    return out << outputKeyIdentity.GetDebugName();
}

static std::ostream &
operator<<(std::ostream &out, const EsfJournal &journal)
{
    if (journal.begin() == journal.end()) {
        return out << "{}";
    }
    out << "{";
    for (const EsfJournal::value_type &entry : journal) {
        out << "\n    <" << entry.first.GetText() << "> "
            << '(' << entry.second.GetDescription() << ')';
    }
    out << "\n}";
    return out;
}

PXR_NAMESPACE_CLOSE_SCOPE

// TestExecInputResolverCustomSchema is a codeless schema that's loaded for this
// test only. The schema is loaded from testenv/testExecInputResolver/resources.
EXEC_REGISTER_SCHEMA(TestExecInputResolverCustomSchema)
{
    self.PrimComputation(_tokens->customComputation)
        .Callback<int>(+[](const VdfContext &){ return 0; });
}

class Fixture
{
public:
    const TfType customSchemaType;
    const Exec_ComputationDefinition *customComputationDefinition;
    EsfJournal journal;

    Fixture()
        : customSchemaType(
            TfType::FindByName("TestExecInputResolverCustomSchema"))
        , customComputationDefinition(
            Exec_DefinitionRegistry::GetInstance()
                .GetPrimComputationDefinition(
                    customSchemaType,
                    _tokens->customComputation))
    {
        TF_AXIOM(!customSchemaType.IsUnknown());
        TF_AXIOM(customComputationDefinition);
    }

    void NewStageFromLayer(const char *layerContents)
    {
        _layer = SdfLayer::CreateAnonymous(".usda");
        _layer->ImportFromString(layerContents);
        TF_AXIOM(_layer);
        _usdStage = UsdStage::Open(_layer);
        TF_AXIOM(_usdStage);
        _esfStage = std::make_unique<EsfStage>(
            ExecUsdSceneAdapter::AdaptStage(_usdStage));
    }

    EsfObject GetObjectAtPath(const char *pathString) const
    {
        return _esfStage->Get()->GetObjectAtPath(SdfPath(pathString), nullptr);
    }

    Exec_OutputKeyVector ResolveInput(
        const EsfObject &origin,
        const TfToken &computationName,
        const TfType resultType,
        const SdfPath &localTraversal,
        const ExecProviderResolution::DynamicTraversal dynamicTraversal)
    {
        const Exec_InputKey inputKey {
            _tokens->inputName,
            computationName,
            resultType,
            ExecProviderResolution {
                localTraversal,
                dynamicTraversal
            }
        };
        return Exec_ResolveInput(origin, inputKey, &journal);
    }

private:
    SdfLayerRefPtr _layer;
    UsdStageConstRefPtr _usdStage;
    // Hold an EsfStage by unique_ptr because it's not default-constructible.
    std::unique_ptr<EsfStage> _esfStage;
};

static void
TestResolveToComputationOrigin(Fixture &fixture)
{
    // Test that Exec_ResolveInput finds a computation on the origin object.

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "Origin" {
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/Origin"),
        _tokens->customComputation,
        TfType::Find<int>(),
        SdfPath("."),
        ExecProviderResolution::DynamicTraversal::Local);

    ASSERT_EQ(outputKeys.size(), 1);
    ASSERT_OUTPUT_KEY(
        outputKeys[0],
        fixture.GetObjectAtPath("/Origin"),
        fixture.customComputationDefinition);

    EsfJournal expectedJournal;
    expectedJournal.Add(SdfPath("/Origin"), EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

static void
TestResolveToComputationOrigin_NoSuchComputation(Fixture &fixture)
{
    // Test that Exec_ResolveInput fails to find a computation on the origin
    // object if that object does not define a computation by that name.

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "Origin" {
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/Origin"),
        _tokens->nonExistentComputation,
        TfType::Find<int>(),
        SdfPath("."),
        ExecProviderResolution::DynamicTraversal::Local);

    ASSERT_EQ(outputKeys.size(), 0);

    EsfJournal expectedJournal;
    expectedJournal.Add(SdfPath("/Origin"), EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

static void
TestResolveToComputationOrigin_WrongResultType(Fixture &fixture)
{
    // Test that Exec_ResolveInput fails to find a computation on the origin
    // object if a computation of the requested name was found, but it does not
    // match the requested result type.

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "Origin" {
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/Origin"),
        _tokens->customComputation,
        TfType::Find<double>(),
        SdfPath("."),
        ExecProviderResolution::DynamicTraversal::Local);

    ASSERT_EQ(outputKeys.size(), 0);

    EsfJournal expectedJournal;
    expectedJournal.Add(SdfPath("/Origin"), EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

static void
TestResolveToNamespaceAncestor(Fixture &fixture)
{
    // Test that Exec_ResolveInput finds a computation on the nearest namespace
    // ancestor that defines the requested computation.

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "Root" {
            def CustomSchema "Ancestor" {
                def Scope "Scope1" {
                    def Scope "Scope2" {
                        def Scope "Origin" {
                        }
                    }
                }
            }
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/Root/Ancestor/Scope1/Scope2/Origin"),
        _tokens->customComputation,
        TfType::Find<int>(),
        SdfPath("."),
        ExecProviderResolution::DynamicTraversal::NamespaceAncestor);

    ASSERT_EQ(outputKeys.size(), 1);
    ASSERT_OUTPUT_KEY(
        outputKeys[0], 
        fixture.GetObjectAtPath("/Root/Ancestor"), 
        fixture.customComputationDefinition);

    EsfJournal expectedJournal;
    expectedJournal
        .Add(SdfPath("/Root/Ancestor/Scope1/Scope2/Origin"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root/Ancestor/Scope1/Scope2"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root/Ancestor/Scope1"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root/Ancestor"),
            EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

static void
TestResolveToNamespaceAncestor_NoSuchAncestor(Fixture &fixture)
{
    // Test that Exec_ResolveInput fails to find a computation on the nearest
    // namespace ancestor if no ancestor defines a computation by that name.

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def Scope "Root" {
            def Scope "Parent" {
                def CustomSchema "Origin" {
                }
            }
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/Root/Parent/Origin"),
        _tokens->customComputation,
        TfType::Find<int>(),
        SdfPath("."),
        ExecProviderResolution::DynamicTraversal::NamespaceAncestor);

    ASSERT_EQ(outputKeys.size(), 0);

    EsfJournal expectedJournal;
    expectedJournal
        .Add(SdfPath("/Root/Parent/Origin"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root/Parent"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root"),
            EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

static void
TestResolveToNamespaceAncestor_WrongResultType(Fixture &fixture)
{
    // Test that Exec_ResolveInput fails to find a computation on the nearest
    // namespace ancestor if all ancestors define computations of the requested
    // name, but of different result types.

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "Root" {
            def CustomSchema "Parent" {
                def CustomSchema "Origin" {
                }
            }
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/Root/Parent/Origin"),
        _tokens->customComputation,
        TfType::Find<double>(),
        SdfPath("."),
        ExecProviderResolution::DynamicTraversal::NamespaceAncestor);

    ASSERT_EQ(outputKeys.size(), 0);

    EsfJournal expectedJournal;
    expectedJournal
        .Add(SdfPath("/Root/Parent/Origin"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root/Parent"),
            EsfEditReason::ResyncedObject)
        .Add(SdfPath("/Root"),
            EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

static void
TestResolveToOwningPrim(Fixture &fixture)
{
    // Test that Exec_ResolveInput finds a computation on the owning prim when
    // the origin is an attribute, and the local traversal is "..".

    fixture.NewStageFromLayer(R"usd(#usda 1.0
        def CustomSchema "OwningPrim" {
            double origin = 1.0
        }
    )usd");

    const Exec_OutputKeyVector outputKeys = fixture.ResolveInput(
        fixture.GetObjectAtPath("/OwningPrim.origin"),
        _tokens->customComputation,
        TfType::Find<int>(),
        SdfPath(".."),
        ExecProviderResolution::DynamicTraversal::Local);

    ASSERT_EQ(outputKeys.size(), 1);
    ASSERT_EQ(outputKeys.size(), 1);
    ASSERT_OUTPUT_KEY(
        outputKeys[0], 
        fixture.GetObjectAtPath("/OwningPrim"), 
        fixture.customComputationDefinition);

    EsfJournal expectedJournal;
    expectedJournal
        .Add(SdfPath("/OwningPrim.origin"), EsfEditReason::ResyncedObject)
        .Add(SdfPath("/OwningPrim"), EsfEditReason::ResyncedObject);
    ASSERT_EQ(fixture.journal, expectedJournal);
}

int main()
{
    // Load the custom schema.
    const PlugPluginPtrVector testPlugins = PlugRegistry::GetInstance()
        .RegisterPlugins(TfAbsPath("resources"));
    TF_AXIOM(testPlugins.size() == 1);
    TF_AXIOM(testPlugins[0]->GetName() == "testExecInputResolver");

    std::vector tests {
        TestResolveToComputationOrigin,
        TestResolveToComputationOrigin_NoSuchComputation,
        TestResolveToComputationOrigin_WrongResultType,
        TestResolveToNamespaceAncestor,
        TestResolveToNamespaceAncestor_NoSuchAncestor,
        TestResolveToNamespaceAncestor_WrongResultType,
        TestResolveToOwningPrim,
    };
    for (const auto &test : tests) {
        Fixture fixture;
        test(fixture);
    }
}
