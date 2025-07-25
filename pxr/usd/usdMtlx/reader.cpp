//
// Copyright 2018-2019 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"
#include "pxr/usd/usdMtlx/debugCodes.h"
#include "pxr/usd/usdMtlx/reader.h"
#include "pxr/usd/usdMtlx/utils.h"
#include "pxr/usd/usdMtlx/materialXConfigAPI.h"
#include "pxr/usd/usdMtlx/tokens.h"

#include "pxr/usd/usdGeom/primvar.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdShade/nodeGraph.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdShade/tokens.h"
#include "pxr/usd/usdShade/utils.h"
#include "pxr/usd/usdUI/nodeGraphNodeAPI.h"
#include "pxr/usd/sdr/declare.h"
#include "pxr/usd/sdf/attributeSpec.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/tokens.h"
#include "pxr/usd/usd/collectionAPI.h"
#include "pxr/usd/usd/editContext.h"
#include "pxr/usd/usd/specializes.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/pathUtils.h"

#include <algorithm>


namespace mx = MaterialX;

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// Attribute name tokens.
struct _AttributeNames {
    using Name = const std::string;

    _AttributeNames() { }

    Name channels         {"channels"};
    Name cms              {"cms"};
    Name cmsconfig        {"cmsconfig"};
    Name collection       {"collection"};
    Name context          {"context"};
    Name default_         {"default"};
    Name doc              {"doc"};
    Name enum_            {"enum"};
    Name enumvalues       {"enumvalues"};
    Name excludegeom      {"excludegeom"};
    Name geom             {"geom"};
    Name helptext         {"helptext"};
    Name includegeom      {"includegeom"};
    Name includecollection{"includecollection"};
    Name inherit          {"inherit"};
    Name interfacename    {"interfacename"};
    Name isdefaultversion {"isdefaultversion"};
    Name look             {"look"};
    Name material         {"material"};
    Name member           {"member"};
    Name nodedef          {"nodedef"};
    Name nodegraph        {"nodegraph"};
    Name nodename         {"nodename"};
    Name node             {"node"};
    Name output           {"output"};
    Name semantic         {"semantic"};
    Name token            {"token"};
    Name type             {"type"};
    Name uicolor          {"uicolor"};
    Name uifolder         {"uifolder"};
    Name uimax            {"uimax"};
    Name uimin            {"uimin"};
    Name uiname           {"uiname"};
    Name value            {"value"};
    Name valuecurve       {"valuecurve"};
    Name valuerange       {"valuerange"};
    Name variant          {"variant"};
    Name variantassign    {"variantassign"};
    Name variantset       {"variantset"};
    Name version          {"version"};
    Name xpos             {"xpos"};
    Name ypos             {"ypos"};
};
static const _AttributeNames names;

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    ((light, "light"))
    ((mtlxRenderContext, "mtlx"))
);

// Returns the name of an element.
template <typename T>
inline
static
const std::string&
_Name(const std::shared_ptr<T>& mtlx)
{
    return mtlx->getName();
}

// Returns the children of type T or any type derived from T.
template <typename T, typename U>
inline
static
std::vector<std::shared_ptr<T>>
_Children(const std::shared_ptr<U>& mtlx)
{
    std::vector<std::shared_ptr<T>> result;
    for (const mx::ElementPtr& child: mtlx->getChildren()) {
        if (auto typed = child->asA<T>()) {
            result.emplace_back(std::move(typed));
        }
    }
    return result;
}

// Returns the children of (exactly) the given category.
template <typename T>
inline
static
std::vector<mx::ElementPtr>
_Children(const std::shared_ptr<T>& mtlx, const std::string& category)
{
    std::vector<mx::ElementPtr> result;
    for (auto& child: mtlx->getChildren()) {
        if (child->getCategory() == category) {
            result.emplace_back(child);
        }
    }
    return result;
}

// A helper that wraps a MaterialX attribute value.  We don't usually
// care if an attribute exists, just that the value isn't empty.  (A
// non-existent attribute returns the empty string.)  A std::string
// has no operator bool or operator! so code would look this this:
//   auto& attr = elem->getAttribute("foo");
//   if (!attr.empty()) ...;
// With this helper we can do this:
//   if (auto& attr = _Attr(elem, "foo")) ...;
// This also allows us to add a check for existence in one place if
// that becomes important later.
class _Attr {
public:
    template <typename T>
    _Attr(const std::shared_ptr<T>& element, const std::string& name)
        : _Attr(element->getAttribute(name)) { }

    explicit operator bool() const      { return !_value.empty(); }
    bool operator!() const              { return _value.empty(); }
    operator const std::string&() const { return _value; }
    const std::string& str() const      { return _value; }
    const char* c_str() const           { return _value.c_str(); }

    std::string::const_iterator begin() const { return _value.begin(); }
    std::string::const_iterator end()   const { return _value.end(); }

private:
    explicit _Attr(const std::string& value)
        : _value(value.empty() ? mx::EMPTY_STRING : value) { }

private:
    const std::string& _value = mx::EMPTY_STRING;
};

// Returns the type of a typed element.
template <typename T>
inline
static
const std::string&
_Type(const std::shared_ptr<T>& mtlx)
{
    return _Attr(mtlx, names.type);
}

// Returns the attribute named \p name on element \p mtlx as a T in \p value
// if possible and returns true, otherwise returns false.
template <typename T>
static
bool
_Value(T* value, const mx::ConstElementPtr& mtlx, const std::string& name)
{
    // Fail if the attribute doesn't exist.  This allows us to distinguish
    // an empty string from a missing string.
    if (!mtlx->hasAttribute(name)) {
        return false;
    }

    try {
        *value = mx::fromValueString<T>(_Attr(mtlx, name));
        return true;
    }
    catch (mx::ExceptionTypeError&) {
    }
    return false;
}

// Convert a MaterialX name into a valid USD name token.
static
TfToken
_MakeName(const std::string& mtlxName)
{
    // A MaterialX name may have a namespace name included,
    // which then will be separated by a colon
    auto colonPos = mtlxName.find(':');
    if (colonPos != std::string::npos) {
        // Replace the colon with "__" to 
        // make a valid USD name token
        std::string modifiedName = mtlxName;
        modifiedName.replace(colonPos, 1, "__");
        return TfToken(modifiedName);
    }
    else {
        return TfToken(mtlxName);
    }
}

// Convert a MaterialX name into a valid USD name token.
static
TfToken
_MakeName(const mx::ConstElementPtr& mtlx)
{
    return mtlx ? _MakeName(_Name(mtlx)) : TfToken();
}

// Create a USD input on connectable that conforms to mtlx.
template <typename C>
static
UsdShadeInput
_MakeInput(C& connectable, const mx::ConstTypedElementPtr& mtlx)
{
    // Get the MaterialX type name.
    auto&& type = _Type(mtlx);
    if (type.empty()) {
        return UsdShadeInput();
    }

    // Get the Sdf type, if any.  If not then use token and we'll set
    // the render type later.
    TfToken renderType;
    auto converted = UsdMtlxGetUsdType(type).valueTypeName;
    if (!converted) {
        converted  = SdfValueTypeNames->Token;
        renderType = TfToken(type);
    }

    // Create the input.
    auto usdInput = connectable.CreateInput(_MakeName(mtlx), converted);

    // Set the render type if necessary.
    if (!renderType.IsEmpty()) {
        usdInput.SetRenderType(renderType);
    }

    return usdInput;
}

// Return the nodedef with node=family, that's type compatible with
// mtlxInterface, and has a compatible version.  If target isn't empty
// then it must also match.  Returns null if there's no such nodedef.
static
mx::ConstNodeDefPtr
_FindMatchingNodeDef(
    const mx::ConstDocumentPtr& mtlxDocument,
    const mx::ConstInterfaceElementPtr& mtlxInterface,
    const std::string& family,
    const std::string& type,
    const SdrVersion& version,
    const std::string& target)
{
    mx::ConstNodeDefPtr result = nullptr;

    for (auto&& mtlxNodeDef: mtlxDocument->getMatchingNodeDefs(family)) {
        // Filter by target.
        if (!mx::targetStringsMatch(target, mtlxNodeDef->getTarget())) {
            continue;
        }

        // Filter by types.
        if (mtlxInterface && !mtlxInterface->hasExactInputMatch(mtlxNodeDef)) {
            continue;
        }

        if (mtlxNodeDef->getType() != type) {
            continue;
        }

        // XXX -- We may want to cache nodedef version info.

        // Filter by version.
        bool implicitDefault;
        auto nodeDefVersion = UsdMtlxGetVersion(mtlxNodeDef,&implicitDefault);
        if (version.IsDefault()) {
            if (implicitDefault) {
                // This nodedef matches if no other nodedef is explicitly
                // the default so save it as the best match so far.
                result = mtlxNodeDef;
            }
            else if (nodeDefVersion.IsDefault()) {
                // The nodedef is explicitly the default and matches.
                result = mtlxNodeDef;
                break;
            }
        }
        else if (version == nodeDefVersion) {
            result = mtlxNodeDef;
            break;
        }
    }

    return result;
}

// Return the shader nodedef with node=family that has a compatible version.
// If target isn't empty
// then it must also match.  Returns null if there's no such nodedef.
// If the nodedef is not found in the document then the standard
// library is also checked.
static
mx::ConstNodeDefPtr
_FindMatchingNodeDef(
    const mx::ConstNodePtr& mtlxShaderNode,
    const std::string& family,
    const SdrVersion& version,
    const std::string& target,
    const mx::ConstInterfaceElementPtr& mtlxInterface = mx::NodeDefPtr())
{
    auto nodeDef = _FindMatchingNodeDef(mtlxShaderNode->getDocument(),
                                        mtlxInterface,
                                        mtlxShaderNode->getCategory(),
                                        mtlxShaderNode->getType(),
                                        UsdMtlxGetVersion(mtlxShaderNode),
                                        mtlxShaderNode->getTarget());
    if (nodeDef) {
        return nodeDef;
    }

    // Get the standard library document and check that.
    static auto standardLibraryDocument = UsdMtlxGetDocument("");

    if (mtlxShaderNode->hasNodeDefString()) {
        nodeDef = standardLibraryDocument->getNodeDef(mtlxShaderNode->getNodeDefString());
        if (nodeDef) {
            return nodeDef;
        }
    }

    return _FindMatchingNodeDef(standardLibraryDocument,
                                mtlxInterface,
                                mtlxShaderNode->getCategory(),
                                mtlxShaderNode->getType(),
                                UsdMtlxGetVersion(mtlxShaderNode),
                                mtlxShaderNode->getTarget());
}

// Get the nodeDef either from the mtlxNode itself or get it from the stdlib.
// For custom nodedefs defined in the loaded mtlx document one should be able to
// get the nodeDef from the node, for all other instances corresponding nodeDefs
// need to be accessed from the stdlib.
static
mx::ConstNodeDefPtr
_GetNodeDef(const mx::ConstNodePtr& mtlxNode)
{
    mx::ConstNodeDefPtr mtlxNodeDef = mtlxNode->getNodeDef();

    if (mtlxNodeDef) {
        return mtlxNodeDef;
    }

    auto mtlxType = mtlxNode->getType();
    if (mtlxType == mx::SURFACE_SHADER_TYPE_STRING ||
        mtlxType == mx::DISPLACEMENT_SHADER_TYPE_STRING ||
        mtlxType == mx::VOLUME_SHADER_TYPE_STRING ||
        mtlxType == mx::LIGHT_SHADER_TYPE_STRING) {
        return _FindMatchingNodeDef(mtlxNode, mtlxNode->getCategory(), 
                                    UsdMtlxGetVersion(mtlxNode),
                                    mtlxNode->getTarget());
    }
    else {
        return _FindMatchingNodeDef(mtlxNode, mtlxNode->getCategory(), 
                                    UsdMtlxGetVersion(mtlxNode),
                                    mtlxNode->getTarget(), mtlxNode);
    }
}

// Get the shader id for a MaterialX nodedef.
static
SdrIdentifier
_GetShaderId(const mx::ConstNodeDefPtr& mtlxNodeDef)
{
    return mtlxNodeDef ? SdrIdentifier(mtlxNodeDef->getName())
                       : SdrIdentifier();
}

// Get the shader id for a MaterialX node.
static
SdrIdentifier
_GetShaderId(const mx::ConstNodePtr& mtlxNode)
{
    return _GetShaderId(_GetNodeDef(mtlxNode));
}

static
bool
_SetColorSpace(const mx::ConstValueElementPtr& mxElem)
{
    const std::string &activeColorSpace = mxElem->getActiveColorSpace();
    const std::string &defaultSourceColorSpace =
        mxElem->getDocument()->getActiveColorSpace();

    // Only need to set the colorSpace on elements whose colorspace differs 
    // from the default source colorSpace.
    return !activeColorSpace.empty() &&
            activeColorSpace != defaultSourceColorSpace;
}
static
bool
_TypeSupportsColorSpace(const mx::ConstValueElementPtr& mxElem)
{
    // ColorSpaces are supported on 
    //  - inputs of type color3 or color4
    //  - filename inputs on image nodes with color3 or color4 outputs
    const std::string &type = mxElem->getType();
    const bool colorInput = type == "color3" || type == "color4";

    bool colorImageNode = false;
    if (type == "filename") {
        mx::ConstNodeDefPtr parentNodeDef;
        if (mxElem->getParent()->isA<mx::Node>()) {
            parentNodeDef = _GetNodeDef(mxElem->getParent()->asA<mx::Node>());
        }
        else if (mxElem->getParent()->isA<mx::NodeDef>()) {
            parentNodeDef = mxElem->getParent()->asA<mx::NodeDef>();
        }

        // Verify the output is color3 or color4
        if (parentNodeDef) {
            for (const mx::OutputPtr& output : parentNodeDef->getOutputs()) {
                const std::string &type = output->getType();
                colorImageNode |= type == "color3" || type == "color4";
            }
        }
    }

    return colorInput || colorImageNode;
}

// Copy the value from a Material value element to a UsdShadeInput with a
// Set() method taking any valid USD value type.
static
void
_CopyValue(const UsdShadeInput& usd, const mx::ConstValueElementPtr& mtlx)
{
    // Check for default value.
    auto value = UsdMtlxGetUsdValue(mtlx);
    if (!value.IsEmpty()) {
        usd.Set(value);
    }

    // Check for animated values.
    auto valuecurve = _Attr(mtlx, names.valuecurve);
    auto valuerange = _Attr(mtlx, names.valuerange);
    if (valuecurve && valuerange) {
        auto values = UsdMtlxGetPackedUsdValues(valuecurve,
                                                _Attr(mtlx, names.type));
        if (!values.empty()) {
            auto range = UsdMtlxGetPackedUsdValues(valuerange, "integer");
            if (range.size() == 2) {
                const int first = range[0].Get<int>();
                const int last  = range[1].Get<int>();
                if (last < first) {
                    TF_WARN(TfStringPrintf(
                        "Invalid valuerange [%d,%d] on '%s';  ignoring",
                        first, last, mtlx->getNamePath().c_str()));
                }
                else if (values.size() != size_t(last - first + 1)) {
                    TF_WARN(TfStringPrintf(
                        "valuerange [%d,%d] doesn't match valuecurve size "
                        "%zd on '%s';  ignoring",
                        first, last, values.size(),
                        mtlx->getNamePath().c_str()));
                }
                else {
                    auto frame = first;
                    for (auto&& value: values) {
                        usd.Set(value, UsdTimeCode(frame++));
                    }
                }
            }
            else {
                TF_WARN(TfStringPrintf(
                    "Malformed valuerange '%s' on '%s';  ignoring",
                    valuerange.c_str(), mtlx->getNamePath().c_str()));
            }
        }
        else {
            TF_WARN(TfStringPrintf(
                "Failed to parse valuecurve '%s' on '%s';  ignoring",
                valuecurve.c_str(), mtlx->getNamePath().c_str()));
        }
    }

    // Set the ColorSpace if needed. 
    if (_SetColorSpace(mtlx) && _TypeSupportsColorSpace(mtlx)) {
        usd.GetAttr().SetColorSpace(TfToken(mtlx->getActiveColorSpace()));
    }
}

// Copies common UI attributes available on any element from the element
// \p mtlx to the object \p usd.
static
void
_SetGlobalCoreUIAttributes(
    const UsdObject& usd, const mx::ConstElementPtr& mtlx)
{
    if (auto doc = _Attr(mtlx, names.doc)) {
        usd.SetDocumentation(doc);
    }
}

// Copies common UI attributes from the element \p mtlx to the object \p usd.
static
void
_SetCoreUIAttributes(const UsdObject& usd, const mx::ConstElementPtr& mtlx)
{
    _SetGlobalCoreUIAttributes(usd, mtlx);

    if (usd.Is<UsdPrim>()) {
        if (auto ui = UsdUINodeGraphNodeAPI(usd.GetPrim())) {
            float xpos, ypos;
            if (_Value(&xpos, mtlx, names.xpos) &&
                    _Value(&ypos, mtlx, names.ypos)) {
                ui.CreatePosAttr(VtValue(GfVec2f(xpos, ypos)));
            }

            mx::Vector3 color;
            if (_Value(&color, mtlx, names.uicolor)) {
                ui.CreateDisplayColorAttr(
                    VtValue(GfVec3f(color[0], color[1], color[2])));
            }
        }
    }
}

// Copies common UI attributes from the element \p mtlx to the object \p usd.
static
void
_SetUIAttributes(const UsdShadeInput& usd, const mx::ConstElementPtr& mtlx)
{
    if (auto helptext = _Attr(mtlx, names.helptext)) {
        usd.SetDocumentation(helptext);
    }

    mx::StringVec enums;
    if (_Value(&enums, mtlx, names.enum_) && !enums.empty()) {
        // We can't write this directly via Usd API except through
        // SetMetadata() with a hard-coded key.  We'll use the Sdf
        // API instead.
        auto attr =
            TfStatic_cast<SdfAttributeSpecHandle>(
                usd.GetAttr().GetPropertyStack().front());
        VtTokenArray allowedTokens;
        allowedTokens.reserve(enums.size());
        for (const auto& tokenString: enums) {
            allowedTokens.push_back(TfToken(tokenString));
        }
        attr->SetAllowedTokens(allowedTokens);

        // XXX -- enumvalues has no USD counterpart
    }

    // XXX -- uimin, uimax have no USD counterparts.

    if (auto uifolder = _Attr(mtlx, names.uifolder)) {
        // Translate '/' to ':'.
        std::string group = uifolder;
        std::replace(group.begin(), group.end(), '/', ':');
        usd.GetAttr().SetDisplayGroup(group);
    }
    if (auto uiname = _Attr(mtlx, names.uiname)) {
        usd.GetAttr().SetDisplayName(uiname);
    }

    _SetCoreUIAttributes(usd.GetAttr(), mtlx);
}

/// Returns an inheritance sequence with the most derived at the end
/// of the sequence.
template <typename T>
static
std::vector<std::shared_ptr<T>>
_GetInheritanceStack(const std::shared_ptr<T>& mtlxMostDerived)
{
    std::vector<std::shared_ptr<T>> result;

    // This is basically InheritanceIterator from 1.35.5 and up.
    std::set<std::shared_ptr<T>> visited;
    auto document = mtlxMostDerived->getDocument();
    for (auto mtlx = mtlxMostDerived; mtlx;
            mtlx = std::dynamic_pointer_cast<T>(
                document->getChild(_Attr(mtlx, names.inherit)))) {
        if (!visited.insert(mtlx).second) {
            throw mx::ExceptionFoundCycle(
                "Encountered cycle at element: " + mtlx->asString());
        }
        result.push_back(mtlx);
    }

    // We want more derived to the right.
    std::reverse(result.begin(), result.end());
    return result;
}

/// Add a Referenced nodegraph prim at the given path, returning:
/// - the prim at the referencingPath, if it exists and is a valid nodegraph
/// - an empty prim, if another prim already exists at the referencingPath
/// - a new referenced prim of the ownerPrim at the referencingPath, if there 
///   is no prim at the referencingPath
static
UsdPrim
_AddReference(const UsdPrim &ownerPrim, const SdfPath &referencingPath)
{
    if (!ownerPrim) {
        return UsdPrim();
    }

    UsdStageWeakPtr stage = ownerPrim.GetStage();
    if (UsdPrim referencedPrim = stage->GetPrimAtPath(referencingPath)) {
        // If a valid nodegraph exists at the referencing path, return that.
        if (UsdShadeNodeGraph(referencedPrim)) {
            return referencedPrim;
        }
        
        if (!referencedPrim.GetTypeName().IsEmpty()) {
            TF_WARN("Can't create node graph at <%s>; a '%s' already exists",
                    referencingPath.GetText(), 
                    referencedPrim.GetTypeName().GetText());
            return UsdPrim();
        }
    }

    // Create a new prim referencing the node graph.
    UsdPrim referencedPrim = stage->DefinePrim(referencingPath);
    referencedPrim.GetReferences().AddInternalReference(ownerPrim.GetPath());
    return referencedPrim;
}

/// This class translates a MaterialX node graph into a USD node graph.
// XXX: Move this class into a separate file to improve reader.cpp readability
class _NodeGraphBuilder {
public:
    using ShaderNamesByOutputName = std::map<std::string, TfToken>;

    _NodeGraphBuilder() = default;
    _NodeGraphBuilder(const _NodeGraphBuilder&) = delete;
    _NodeGraphBuilder(_NodeGraphBuilder&&) = delete;
    _NodeGraphBuilder& operator=(const _NodeGraphBuilder&) = delete;
    _NodeGraphBuilder& operator=(_NodeGraphBuilder&&) = delete;
    ~_NodeGraphBuilder() = default;

    void SetNodeDefInterface(const mx::ConstNodeDefPtr&);
    void SetContainer(const mx::ConstElementPtr&);
    void SetTarget(const UsdStagePtr&, const SdfPath& targetPath);
    void SetTarget(const UsdStagePtr&, const SdfPath& parentPath,
                   const mx::ConstElementPtr& childName);
    UsdPrim Build(ShaderNamesByOutputName* outputs);

private:
    void _CreateInterfaceInputs(const mx::ConstInterfaceElementPtr &iface,
                                const UsdShadeConnectableAPI &connectable);
    bool _IsLocalCustomNode(const mx::ConstNodeDefPtr &mtlxNodeDef);
    void _AddNode(const mx::ConstNodePtr &mtlxNode, const UsdPrim &usdParent);
    UsdShadeInput _AddInput(const mx::ConstInputPtr& mtlxInput,
                            const UsdShadeConnectableAPI& connectable,
                            bool isInterface = false);
    UsdShadeInput _AddInputCommon(const mx::ConstValueElementPtr& mtlxValue,
                                  const UsdShadeConnectableAPI& connectable,
                                  bool isInterface);
    UsdShadeOutput _AddOutput(const mx::ConstTypedElementPtr& mtlxTyped,
                              const mx::ConstElementPtr& mtlxOwner,
                              const UsdShadeConnectableAPI& connectable,
                              bool shaderOnly = false);
    template <typename D>
    void _ConnectPorts(const mx::ConstPortElementPtr& mtlxDownstream,
                       const D& usdDownstream);
    template <typename U, typename D>
    void _ConnectPorts(const mx::ConstElementPtr& mtlxDownstream,
                       const U& usdUpstream, const D& usdDownstream);
    void _ConnectNodes();
    void _ConnectTerminals(const mx::ConstElementPtr& iface,
                           const UsdShadeConnectableAPI& connectable);

private:
    mx::ConstNodeDefPtr _mtlxNodeDef;
    mx::ConstElementPtr _mtlxContainer;
    UsdStagePtr _usdStage;
    SdfPath _usdPath;
    std::map<std::string, UsdShadeInput> _interfaceNames;
    std::map<mx::ConstInputPtr, UsdShadeInput> _inputs;
    std::map<std::string, std::vector<UsdShadeOutput>> _outputs;
};

void
_NodeGraphBuilder::SetNodeDefInterface(const mx::ConstNodeDefPtr& mtlxNodeDef)
{
    _mtlxNodeDef = mtlxNodeDef;
}

void
_NodeGraphBuilder::SetContainer(const mx::ConstElementPtr& mtlxContainer)
{
    _mtlxContainer = mtlxContainer;
}

void
_NodeGraphBuilder::SetTarget(const UsdStagePtr& stage, const SdfPath& path)
{
    _usdStage = stage;
    _usdPath  = path;
}

void
_NodeGraphBuilder::SetTarget(
    const UsdStagePtr& stage,
    const SdfPath& parentPath,
    const mx::ConstElementPtr& childName)
{
    SetTarget(stage, parentPath.AppendChild(_MakeName(childName)));
}

UsdPrim
_NodeGraphBuilder::Build(ShaderNamesByOutputName* outputs)
{
    if (!TF_VERIFY(_usdStage)) {
        return UsdPrim();
    }
    if (!TF_VERIFY(_usdPath.IsAbsolutePath() && _usdPath.IsPrimPath())) {
        return UsdPrim();
    }

    // Create a USD nodegraph.
    auto usdNodeGraph = UsdShadeNodeGraph::Define(_usdStage, _usdPath);
    if (!usdNodeGraph) {
        return UsdPrim();
    }
    UsdPrim usdPrim = usdNodeGraph.GetPrim();

    const bool isExplicitNodeGraph = _mtlxContainer->isA<mx::NodeGraph>();
    if (isExplicitNodeGraph) {
        _SetCoreUIAttributes(usdPrim, _mtlxContainer);

        // Create the interface inputs for the NodeDef.
        if (_mtlxNodeDef) {
            for (mx::ConstNodeDefPtr& nd : _GetInheritanceStack(_mtlxNodeDef)) {
                _CreateInterfaceInputs(nd, usdNodeGraph.ConnectableAPI());
            }
        }

        // Add Nodegraph Inputs.
        for (mx::InputPtr in : _mtlxContainer->getChildrenOfType<mx::Input>()) {
            // Note nodegraph inputs are referenced inside the nodegraph with
            // the 'interfacename' attribute name within the Mtlx Document
            _AddInput(in, usdNodeGraph.ConnectableAPI(), /* isInterface */ true);
        }
    }

    // Build the graph of nodes.
    for (mx::NodePtr& mtlxNode : _mtlxContainer->getChildrenOfType<mx::Node>()) {
        // If the _mtlxContainer is the document (there is no nodegraph) the 
        // nodes gathered here will include the material and surfaceshader 
        // nodes which are not part of the implicit nodegraph. Ignore them.
        const std::string &nodeType = _Attr(mtlxNode, names.type);
        if (nodeType == "material" || nodeType == "surfaceshader")
            continue;
        _AddNode(mtlxNode, usdPrim);
    }
    _ConnectNodes();
    _ConnectTerminals(_mtlxContainer, UsdShadeConnectableAPI(usdPrim));

    return usdPrim;
}

void
_NodeGraphBuilder::_CreateInterfaceInputs(
    const mx::ConstInterfaceElementPtr &iface,
    const UsdShadeConnectableAPI &connectable)
{
    static constexpr bool isInterface = true;

    for (mx::InputPtr mtlxInput: iface->getInputs()) {
        _AddInput(mtlxInput, connectable, isInterface);
    }
    // We deliberately ignore tokens here.
}

// Returns True if the mtlxNodeDef corresponds to a locally defined custom node
// with an associated nodegraph.
// XXX Locally defined custom nodes without nodegraphs are not supported
bool
_NodeGraphBuilder::_IsLocalCustomNode(const mx::ConstNodeDefPtr &mtlxNodeDef)
{
    if (!mtlxNodeDef) {
        return false;
    }

    // Get the absolute path to the NodeDef source uri
    std::string nodeDefUri = UsdMtlxGetSourceURI(mtlxNodeDef);
    if (TfIsRelativePath(nodeDefUri)) {
        // Get the absolute path to the base mtlx file and strip the filename
        std::string fullMtlxPath = UsdMtlxGetSourceURI(mtlxNodeDef->getParent());
        std::size_t found = fullMtlxPath.rfind("/");
        if (found != std::string::npos) {
            fullMtlxPath = fullMtlxPath.substr(0, found+1);
        }
        // Combine with the nodeDef relative path
        nodeDefUri = TfNormPath(fullMtlxPath + nodeDefUri);
    }
    
    // This is a locally defined custom node if the absolute path to the
    // nodedef is not included in the stdlibDoc.
    static mx::StringSet customNodeDefNames;
    static const mx::StringSet stdlibIncludes =
        UsdMtlxGetDocument("")->getReferencedSourceUris();
    if (stdlibIncludes.find(nodeDefUri) == stdlibIncludes.end()) {
        // Check if we already used this custom node
        if (std::find(customNodeDefNames.begin(), customNodeDefNames.end(),
            mtlxNodeDef->getName()) != customNodeDefNames.end()) {
            return true;
        }
        // Verify we have an associated nodegraph, since only locally defined 
        // custom nodes with nodegraphs (not implementations) are supported.  
        if (mx::InterfaceElementPtr impl = mtlxNodeDef->getImplementation()) {
            if (impl && impl->isA<mx::NodeGraph>()) {
                customNodeDefNames.insert(mtlxNodeDef->getName());
                return true;
            }
        }
        TF_WARN("Locally defined custom nodes without nodegraph implementations"
                " are not currently supported.");
    }
    return false;
}

void
_NodeGraphBuilder::_AddNode(
    const mx::ConstNodePtr &mtlxNode,
    const UsdPrim &usdParent)
{
    // Create the shader.
    SdrIdentifier shaderId = _GetShaderId(mtlxNode);
    if (shaderId.IsEmpty()) {
        // If we don't have an interface then this is okay.
        if (_mtlxNodeDef) {
            return;
        }
    }

    UsdStageWeakPtr usdStage = usdParent.GetStage();
    const mx::ConstNodeDefPtr mtlxNodeDef = _GetNodeDef(mtlxNode);

    // If this is a locally defined custom mtlx node, use the associated
    // UsdShadeNodeGraph as the connectable, otherwise use the UsdShadeShader 
    // version of the mtlxNode.
    UsdShadeConnectableAPI connectable;
    if (_IsLocalCustomNode(mtlxNodeDef)) {
        TF_DEBUG(USDMTLX_READER).Msg("Processing custom node (%s) of def (%s) "
                "to be added alongside nodegraph (%s).\n", 
                mtlxNode->getName().c_str(),
                mtlxNodeDef->getName().c_str(),
                usdParent.GetPath().GetText());
        // Nodegraphs associated with locally defined custom nodes are added 
        // before reading materials, and therefore get-able here 
        auto nodeGraphPath = usdParent.GetParent().GetPath().AppendChild(
            _MakeName(mtlxNodeDef));
        auto usdNodeGraph = UsdShadeNodeGraph::Get(usdStage, nodeGraphPath);
        connectable = usdNodeGraph.ConnectableAPI();
        _SetCoreUIAttributes(usdNodeGraph.GetPrim(), mtlxNode);
    }
    else {
        TF_DEBUG(USDMTLX_READER).Msg("Processing shader node (%s) to be added "
                "under parent (%s).\n", mtlxNode->getName().c_str(), 
                usdParent.GetPath().GetText());
        auto shaderPath = usdParent.GetPath().AppendChild(_MakeName(mtlxNode));
        auto usdShader  = UsdShadeShader::Define(usdStage, shaderPath);
        if (!shaderId.IsEmpty()) {
            usdShader.CreateIdAttr(VtValue(TfToken(shaderId)));
        }
        connectable = usdShader.ConnectableAPI();
        _SetCoreUIAttributes(usdShader.GetPrim(), mtlxNode);
    }

    // Add the inputs.
    for (mx::InputPtr mtlxInput: mtlxNode->getInputs()) {
        _AddInput(mtlxInput, connectable);
    }

    // We deliberately ignore tokens here.

    // Add the outputs.
    if (mtlxNodeDef) {
        for (mx::ConstNodeDefPtr nd : _GetInheritanceStack(mtlxNodeDef)) {
            for (mx::OutputPtr mtlxOutput: nd->getOutputs()) {
                _AddOutput(mtlxOutput, mtlxNode, connectable);
            }
        }
    }
    else {
        // Do not add any (default) output to the usd node if the mtlxNode
        // is missing a corresponding mtlxNodeDef.
        TF_WARN("Unable to find the nodedef for '%s' node, outputs not added.",
                mtlxNode->getName().c_str());
    }
}

UsdShadeInput
_NodeGraphBuilder::_AddInput(
    const mx::ConstInputPtr& mtlxInput,
    const UsdShadeConnectableAPI& connectable,
    bool isInterface)
{
    return _inputs[mtlxInput] =
        _AddInputCommon(mtlxInput, connectable, isInterface);
}

UsdShadeInput
_NodeGraphBuilder::_AddInputCommon(
    const mx::ConstValueElementPtr& mtlxValue,
    const UsdShadeConnectableAPI& connectable,
    bool isInterface)
{
    TF_DEBUG(USDMTLX_READER).Msg("Adding input (%s) to connectable prim: (%s)\n",
            mtlxValue->getName().c_str(), 
            connectable.GetPrim().GetPath().GetText());
    auto usdInput = _MakeInput(connectable, mtlxValue);

    _CopyValue(usdInput, mtlxValue);
    _SetUIAttributes(usdInput, mtlxValue);

    // Add to the interface.
    if (isInterface) {
        _interfaceNames[_Name(mtlxValue)] = usdInput;
    }
    else {
        // See if this input is connected to the interface.
        if (auto name = _Attr(mtlxValue, names.interfacename)) {
            auto i = _interfaceNames.find(name);
            if (i != _interfaceNames.end()) {
                _ConnectPorts(mtlxValue, i->second, usdInput);
            }
            else {
                TF_WARN("No interface name '%s' for node '%s'",
                        name.c_str(), _Name(mtlxValue).c_str());
            }
        }
    }

    return usdInput;
}

UsdShadeOutput
_NodeGraphBuilder::_AddOutput(
    const mx::ConstTypedElementPtr& mtlxTyped,
    const mx::ConstElementPtr& mtlxOwner,
    const UsdShadeConnectableAPI& connectable,
    bool shaderOnly)
{
    auto& mtlxType = _Type(mtlxTyped);

    // Get the context, if any.
    std::string context;
    auto mtlxTypeDef = mtlxTyped->getDocument()->getTypeDef(mtlxType);
    if (mtlxTypeDef) {
        if (auto semantic = _Attr(mtlxTypeDef, names.semantic)) {
            if (semantic.str() == mx::SHADER_SEMANTIC) {
                context = _Attr(mtlxTypeDef, names.context);
            }
        }
    }

    // Choose the type.  USD uses Token for shader semantic types.
    TfToken renderType;
    SdfValueTypeName usdType;
    if (context == "surface" ||
            context == "displacement" ||
            context == "volume" ||
            context == "light" ||
            mtlxType == mx::SURFACE_SHADER_TYPE_STRING ||
            mtlxType == mx::DISPLACEMENT_SHADER_TYPE_STRING ||
            mtlxType == mx::VOLUME_SHADER_TYPE_STRING ||
            mtlxType == mx::LIGHT_SHADER_TYPE_STRING) {
        usdType = SdfValueTypeNames->Token;
    }
    else if (shaderOnly || !context.empty()) {
        // We don't know this shader semantic MaterialX type so use Token.
        usdType = SdfValueTypeNames->Token;
    }
    else {
        usdType = UsdMtlxGetUsdType(mtlxType).valueTypeName;
        if (!usdType) {
            usdType = SdfValueTypeNames->Token;
            renderType = TfToken(mtlxType);
        }
    }

    const auto outputName = _MakeName(mtlxTyped);

    // Get the node name.
    auto& nodeName = _Name(mtlxOwner);

    // Compute a key for finding this output.
    auto key = nodeName;

    auto result = connectable.CreateOutput(outputName, usdType);
    auto it = _outputs.insert(
        std::pair<std::string, std::vector<UsdShadeOutput>>(key, {result}));
    if (!it.second) {
        it.first->second.push_back(result);
    }

    if (!renderType.IsEmpty()) {
        result.SetRenderType(renderType);
    }
    _SetCoreUIAttributes(result.GetAttr(), mtlxTyped);
    return result;
}

template <typename D>
void
_NodeGraphBuilder::_ConnectPorts(
    const mx::ConstPortElementPtr& mtlxDownstream,
    const D& usdDownstream)
{
    if (auto nodeName = _Attr(mtlxDownstream, names.nodename)) {
        auto i = _outputs.find(nodeName.str());
        if (i == _outputs.end()) {
            TF_WARN("Output for <%s> missing",
                    usdDownstream.GetAttr().GetPath().GetText());
            return;
        }

        // If the downstream node has multiple outpts, use the output attribute
        // on the mtlxDownstream node to connect to the correct UsdShadeOutput
        if (i->second.size() > 1) {
            UsdShadeOutput downstreamOutput;
            if (auto outputName = _Attr(mtlxDownstream, names.output)) {
                for (const UsdShadeOutput &out : i->second) {
                    if (out.GetBaseName() == TfToken(outputName)) {
                        downstreamOutput = out;
                        break;
                    }
                }
            }
            _ConnectPorts(mtlxDownstream, downstreamOutput, usdDownstream);
        }
        else {
            _ConnectPorts(mtlxDownstream, i->second[0], usdDownstream);
        }
    }
}

template <typename U, typename D>
void
_NodeGraphBuilder::_ConnectPorts(
    const mx::ConstElementPtr& mtlxDownstream,
    const U& usdUpstream,
    const D& usdDownstream)
{
    if (mx::ConstInputPtr mtlxInput = mtlxDownstream->asA<mx::Input>()) {
        if (auto member = _Attr(mtlxInput, names.member)) {
            // XXX -- MaterialX member support.
            TF_WARN("Dropped member %s between <%s> -> <%s>",
                    member.c_str(),
                    usdUpstream.GetAttr().GetPath().GetText(),
                    usdDownstream.GetAttr().GetPath().GetText());
        }

        if (auto channels = _Attr(mtlxInput, names.channels)) {
            // XXX -- MaterialX swizzle support.
            TF_WARN("Dropped swizzle %s between <%s> -> <%s>",
                    channels.c_str(),
                    usdUpstream.GetAttr().GetPath().GetText(),
                    usdDownstream.GetAttr().GetPath().GetText());
        }
    }

    TF_DEBUG(USDMTLX_READER).Msg(" - Getting referencedPrim for (%s) under "
            "(%s).\n", usdUpstream.GetAttr().GetPath().GetText(),
            usdDownstream.GetAttr().GetPath().GetText());

    SdfPath sourcePath = usdUpstream.GetAttr().GetPath();
    const UsdPrim &downstreamPrim = usdDownstream.GetPrim();
    const UsdPrim &upstreamPrim = usdUpstream.GetPrim();

    // Make sure usdUpstream is within scope of usdDownstream before conecting
    // to fullfill the UsdShade encapsulation rule.
    // Note that this is used only for scenarios where the usdUpstream prim 
    // is a nodegraph representing a mtlx custom node. If the existing 
    // usdUpstream prim is a parent of the usdDownstream prim, encapsulation
    // is guaranteed and we do not need to create a reference.
    if (downstreamPrim.GetParent() != upstreamPrim &&
            UsdShadeNodeGraph(upstreamPrim)) {
        // If downstreamPrim is a shader, make sure to use its parent path to 
        // construct the referencePath since Shader nodes are not containers.
        const SdfPath &downstreamPath =
            downstreamPrim.IsA<UsdShadeShader>()
                ? downstreamPrim.GetParent().GetPath()
                : downstreamPrim.GetPath();
        const SdfPath &upstreamPath = downstreamPath.AppendChild(
                upstreamPrim.GetPath().GetNameToken());

        UsdPrim referencedPrim = _AddReference(upstreamPrim, upstreamPath);
        sourcePath = referencedPrim.GetPath().AppendProperty(
                usdUpstream.GetAttr().GetPath().GetNameToken());
    }

    // Connect.
    if (!usdDownstream.ConnectToSource(sourcePath)) {
        TF_WARN("Failed to connect <%s> -> <%s>",
                sourcePath.GetText(),
                usdDownstream.GetAttr().GetPath().GetText());
    }
    else {
        TF_DEBUG(USDMTLX_READER).Msg("    + Connected <%s> -> <%s>\n",
                sourcePath.GetText(),
                usdDownstream.GetAttr().GetPath().GetText());
    }
}

void
_NodeGraphBuilder::_ConnectNodes()
{
    for (std::pair<const mx::ConstInputPtr, UsdShadeInput> &i : _inputs) {
        _ConnectPorts(i.first, i.second);
    }
}

void
_NodeGraphBuilder::_ConnectTerminals(
    const mx::ConstElementPtr& iface,
    const UsdShadeConnectableAPI& connectable)
{
    for (auto& mtlxOutput: iface->getChildrenOfType<mx::Output>()) {
        _ConnectPorts(mtlxOutput, _AddOutput(mtlxOutput, iface, connectable));
    }
}

/// This wraps a UsdNodeGraph to allow referencing which is needed to maintain
/// UsdShade encapsulation rules.
/// XXX This should be moved along with _NodeGraphBuilder to a separate file.
class _NodeGraph {
public:
    _NodeGraph();

    explicit operator bool() const { return bool(_usdOwnerPrim); }

    void SetImplementation(_NodeGraphBuilder&);

    _NodeGraph AddReference(const SdfPath& referencingPath) const;

    UsdPrim GetOwnerPrim() const { return _usdOwnerPrim; }
    UsdShadeOutput GetOutputByName(const std::string& name) const;

private:
    _NodeGraph(const _NodeGraph&, const UsdPrim& referencer);

private:
    UsdPrim _usdOwnerPrim;
    _NodeGraphBuilder::ShaderNamesByOutputName _outputs;
    SdfPath _referencer;
};

_NodeGraph::_NodeGraph()
{
    // Do nothing
}

_NodeGraph::_NodeGraph(const _NodeGraph& other, const UsdPrim& referencer)
    : _usdOwnerPrim(other._usdOwnerPrim)
    , _outputs(other._outputs)
    , _referencer(referencer.GetPath())
{
    // Do nothing
}

void
_NodeGraph::SetImplementation(_NodeGraphBuilder& builder)
{
    if (auto usdOwnerPrim = builder.Build(&_outputs)) {
        // Success.  Cut over.
        _usdOwnerPrim = usdOwnerPrim;
        _referencer   = SdfPath();
    }
}

UsdShadeOutput
_NodeGraph::GetOutputByName(const std::string& name) const
{
    auto nodeGraph =
        _referencer.IsEmpty()
            ? UsdShadeNodeGraph(_usdOwnerPrim)
            : UsdShadeNodeGraph::Get(_usdOwnerPrim.GetStage(), _referencer);
    if (nodeGraph) {
        return nodeGraph.GetOutput(TfToken(name));
    }

    // If this is an implicit node graph then the output is on a
    // child shader.
    auto i = _outputs.find(name);
    if (i != _outputs.end()) {
        auto child =
            _referencer.IsEmpty()
                ? UsdShadeShader(_usdOwnerPrim.GetChild(i->second))
                : UsdShadeShader::Get(_usdOwnerPrim.GetStage(),
                                      _referencer.AppendChild(i->second));
        if (child) {
            return child.GetOutput(UsdMtlxTokens->DefaultOutputName);
        }
    }

    return UsdShadeOutput();
}

_NodeGraph
_NodeGraph::AddReference(const SdfPath& referencingPath) const
{
    if (!_usdOwnerPrim) {
        return *this;
    }

    UsdPrim referencedPrim = _AddReference(_usdOwnerPrim, referencingPath);
    if (referencedPrim) {
        return _NodeGraph(*this, referencedPrim);
    }

    return _NodeGraph();
}

/// This class maintains significant state about the USD stage and
/// provides methods to translate MaterialX elements to USD objects.
/// It also provides enough accessors to implement the reader.
class _Context {
public:
    using VariantName = std::string;
    using VariantSetName = std::string;
    using VariantSetOrder = std::vector<VariantSetName>;

    _Context(const UsdStagePtr& stage, const SdfPath& internalPath);

    void AddVariants(const mx::ConstElementPtr& mtlx);
    _NodeGraph AddNodeGraph(const mx::ConstNodeGraphPtr& mtlxNodeGraph);
    _NodeGraph AddImplicitNodeGraph(const mx::ConstDocumentPtr& mtlxDocument);
    _NodeGraph AddNodeGraphWithDef(const mx::ConstNodeGraphPtr& mtlxNodeGraph);
    UsdShadeMaterial BeginMaterial(const mx::ConstNodePtr& mtlxMaterial);
    void EndMaterial();
    UsdShadeShader AddShaderNode(const mx::ConstNodePtr& mtlxShaderNode);
    void AddMaterialVariant(const std::string& mtlxMaterialName,
                            const VariantSetName& variantSetName,
                            const VariantName& variantName) const;
    UsdCollectionAPI AddCollection(const mx::ConstCollectionPtr&);
    UsdCollectionAPI AddGeometryReference(const mx::ConstGeomElementPtr&);

    const VariantSetOrder& GetVariantSetOrder() const;
    std::set<VariantName> GetVariants(const VariantSetName&) const;
    UsdShadeMaterial GetMaterial(const std::string& mtlxMaterialName) const;
    SdfPath GetCollectionsPath() const;
    UsdCollectionAPI GetCollection(const mx::ConstGeomElementPtr&,
                                   const UsdPrim& prim = UsdPrim()) const;

private:
    using Variant = std::map<std::string, mx::ConstValueElementPtr>;
    using VariantSet = std::map<VariantName, Variant>;
    using VariantSetsByName = std::map<VariantSetName, VariantSet>;

    // A 'collection' attribute key is the collection name.
    using _CollectionKey = std::string;

    // A 'geom' attribute key is the (massaged) geom expressions.
    using _GeomKey = std::string;

    _NodeGraph _AddNodeGraph(const mx::ConstNodeGraphPtr& mtlxNodeGraph,
                             const mx::ConstDocumentPtr& mtlxDocument);
    void _BindNodeGraph(const mx::ConstInputPtr& mtlxInput,
                        const SdfPath& referencingPathParent,
                        const UsdShadeConnectableAPI& connectable,
                        const _NodeGraph& usdNodeGraph);
    static
    UsdShadeInput _AddInput(const mx::ConstValueElementPtr& mtlxValue,
                            const UsdShadeConnectableAPI& connectable);
    static
    UsdShadeInput _AddInputWithValue(const mx::ConstValueElementPtr& mtlxValue,
                                     const UsdShadeConnectableAPI& connectable);
    static
    UsdShadeOutput _AddShaderOutput(const mx::ConstTypedElementPtr& mtlxTyped,
                                    const UsdShadeConnectableAPI& connectable);
    UsdCollectionAPI _AddCollection(const mx::ConstCollectionPtr& mtlxCollection,
                                    std::set<mx::ConstCollectionPtr>* visited);
    UsdCollectionAPI _AddGeomExpr(const mx::ConstGeomElementPtr& mtlxGeomElement);
    void _AddGeom(const UsdRelationship& rel,
                  const std::string& pathString) const;

    const Variant*
    _GetVariant(const VariantSetName&, const VariantName&) const;
    void _CopyVariant(const UsdShadeConnectableAPI&, const Variant&) const;

private:
    UsdStagePtr _stage;
    SdfPath _collectionsPath;
    SdfPath _looksPath;
    SdfPath _materialsPath;
    SdfPath _nodeGraphsPath;
    SdfPath _shadersPath;

    // Global state.
    VariantSetsByName _variantSets;
    VariantSetOrder _variantSetGlobalOrder;
    std::map<mx::ConstNodeGraphPtr, _NodeGraph> _nodeGraphs;
    std::map<std::string, UsdShadeMaterial> _materials;
    std::map<_CollectionKey, UsdCollectionAPI> _collections;
    std::map<_GeomKey, UsdCollectionAPI> _geomSets;
    std::map<mx::ConstGeomElementPtr, UsdCollectionAPI> _collectionMapping;
    // Mapping of MaterialX material name to mapping of shaderNode name to
    // the corresponding UsdShadeShader.  If the shaderNode name is empty
    // this maps to the UsdShadeMaterial.
    std::map<std::string,
             std::map<std::string, UsdShadeConnectableAPI>> _shaders;
    int _nextGeomIndex = 1;

    // Active state.
    mx::ConstNodePtr _mtlxMaterial;
    UsdShadeMaterial _usdMaterial;
};

_Context::_Context(const UsdStagePtr& stage, const SdfPath& internalPath)
    : _stage(stage)
    , _collectionsPath(internalPath.AppendChild(TfToken("Collections")))
    , _looksPath(internalPath.AppendChild(TfToken("Looks")))
    , _materialsPath(internalPath.AppendChild(TfToken("Materials")))
    , _nodeGraphsPath(internalPath.AppendChild(TfToken("NodeGraphs")))
    , _shadersPath(internalPath.AppendChild(TfToken("Shaders")))
{
    // Do nothing
}

void
_Context::AddVariants(const mx::ConstElementPtr& mtlx)
{
    // Collect all of the MaterialX variants.
    for (auto& mtlxVariantSet: _Children(mtlx, names.variantset)) {
        VariantSet variantSet;

        // Over all variants.
        for (auto& mtlxVariant: _Children(mtlxVariantSet, names.variant)) {
            Variant variant;

            // Over all values in the variant.
            for (auto& mtlxValue: _Children<mx::ValueElement>(mtlxVariant)) {
                variant[_Name(mtlxValue)] = mtlxValue;
            }

            // Keep the variant iff there was something in it.
            if (!variant.empty()) {
                variantSet[_Name(mtlxVariant)] = std::move(variant);
            }
        }

        // Keep the variant set iff there was something in it.
        if (!variantSet.empty()) {
            auto& variantSetName = _Name(mtlxVariantSet);
            _variantSets[variantSetName] = std::move(variantSet);
            _variantSetGlobalOrder.push_back(variantSetName);
        }
    }
}

_NodeGraph
_Context::AddNodeGraph(const mx::ConstNodeGraphPtr& mtlxNodeGraph)
{
    return _AddNodeGraph(mtlxNodeGraph, mtlxNodeGraph->getDocument());
}

_NodeGraph
_Context::AddImplicitNodeGraph(const mx::ConstDocumentPtr& mtlxDocument)
{
    return _AddNodeGraph(nullptr, mtlxDocument);
}

_NodeGraph
_Context::_AddNodeGraph(
    const mx::ConstNodeGraphPtr& mtlxNodeGraph,
    const mx::ConstDocumentPtr& mtlxDocument)
{
    auto& nodeGraph = _nodeGraphs[mtlxNodeGraph];
    if (!nodeGraph) {
        _NodeGraphBuilder builder;

        // Choose USD parent path.  If mtlxNodeGraph exists then use
        // its name as the USD nodegraph's name, otherwise we're
        // getting nodes and outputs at the document scope and we
        // don't make a USD nodegraph.
        if (mtlxNodeGraph) {
            TF_DEBUG(USDMTLX_READER).Msg("Add node graph: %s at path %s\n", 
                                         mtlxNodeGraph->getName().c_str(),
                                         _nodeGraphsPath.GetString().c_str());
            builder.SetContainer(mtlxNodeGraph);
            builder.SetTarget(_stage, _nodeGraphsPath, mtlxNodeGraph);
        }
        else {
            TF_DEBUG(USDMTLX_READER).Msg("Add implicit node graph at path %s\n", 
                    _nodeGraphsPath.GetString().c_str());
            builder.SetContainer(mtlxDocument);
            builder.SetTarget(_stage, _nodeGraphsPath);
        }

        nodeGraph.SetImplementation(builder);
    }
    return nodeGraph;
}

_NodeGraph
_Context::AddNodeGraphWithDef(const mx::ConstNodeGraphPtr& mtlxNodeGraph)
{
    auto& nodeGraph = _nodeGraphs[mtlxNodeGraph];
    if (!nodeGraph && mtlxNodeGraph) {
        if (auto mtlxNodeDef = mtlxNodeGraph->getNodeDef()) {
            TF_DEBUG(USDMTLX_READER).Msg("Add mtlxNodeDef %s\n", 
                                         mtlxNodeDef->getName().c_str());
            _NodeGraphBuilder builder;
            builder.SetNodeDefInterface(mtlxNodeDef);
            builder.SetContainer(mtlxNodeGraph);
            builder.SetTarget(_stage, _nodeGraphsPath, mtlxNodeDef);
            nodeGraph.SetImplementation(builder);
        }
    }
    return nodeGraph;
}

UsdShadeMaterial
_Context::BeginMaterial(const mx::ConstNodePtr& mtlxMaterial)
{
    if (TF_VERIFY(!_usdMaterial)) {
        auto materialPath =
            _materialsPath.AppendChild(_MakeName(mtlxMaterial));
        if (auto usdMaterial = UsdShadeMaterial::Define(_stage, materialPath)) {
            // Store the MaterialX document version on the created prim.
            auto mtlxConfigAPI =
                UsdMtlxMaterialXConfigAPI::Apply(usdMaterial.GetPrim());
            auto mtlxVersionStr =
                mtlxMaterial->getDocument()->getVersionString();
            mtlxConfigAPI.CreateConfigMtlxVersionAttr(VtValue(mtlxVersionStr));

            _SetCoreUIAttributes(usdMaterial.GetPrim(), mtlxMaterial);

            // Record the material for later variants.
            _shaders[_Name(mtlxMaterial)][""] =
                UsdShadeConnectableAPI(usdMaterial);

            // Cut over.
            _mtlxMaterial = mtlxMaterial;
            _usdMaterial  = usdMaterial;
        }
    }
    return _usdMaterial;
}

void
_Context::EndMaterial()
{
    if (!TF_VERIFY(_usdMaterial)) {
        return;
    }

    _materials[_Name(_mtlxMaterial)] = _usdMaterial;
    _mtlxMaterial = nullptr;
    _usdMaterial  = UsdShadeMaterial();
}

UsdShadeShader
_Context::AddShaderNode(const mx::ConstNodePtr& mtlxShaderNode)
{
    if (!TF_VERIFY(_usdMaterial)) {
        return UsdShadeShader();
    }

    // Get the nodeDef for this shaderNode.
    mx::ConstNodeDefPtr mtlxNodeDef = mtlxShaderNode->getNodeDef();
    if (!mtlxNodeDef) {
        // The shaderNode specified a node instead of a nodeDef. Find
        // the best matching nodedef since the MaterialX API doesn't.
        mtlxNodeDef =
            _FindMatchingNodeDef(mtlxShaderNode,
                                 mtlxShaderNode->getCategory(),
                                 UsdMtlxGetVersion(mtlxShaderNode),
                                 mtlxShaderNode->getTarget(),
                                 mtlxShaderNode);
    }
    auto shaderId = _GetShaderId(mtlxNodeDef);
    if (shaderId.IsEmpty()) {
        return UsdShadeShader();
    }

    const auto name = _MakeName(mtlxShaderNode);

    // Create the shader if it doesn't exist and copy node def values.
    auto shaderImplPath = _shadersPath.AppendChild(name);
    if (auto usdShaderImpl = UsdShadeShader::Get(_stage, shaderImplPath)) {
        // Do nothing
    }
    else if ((usdShaderImpl = UsdShadeShader::Define(_stage, shaderImplPath))) {
        TF_DEBUG(USDMTLX_READER).Msg("Created shader mtlx %s, as usd %s\n",
                                     mtlxNodeDef->getName().c_str(),
                                     name.GetString().c_str());
        usdShaderImpl.CreateIdAttr(VtValue(TfToken(shaderId)));
        auto connectable = usdShaderImpl.ConnectableAPI();
        _SetCoreUIAttributes(usdShaderImpl.GetPrim(), mtlxShaderNode);

        for (auto& i: _GetInheritanceStack(mtlxNodeDef)) {
            // Create USD output(s) for each MaterialX output with
            // semantic="shader".
            for (auto mtlxOutput: i->getOutputs()) {
                _AddShaderOutput(mtlxOutput, connectable);
            }
        }
    }

    // Reference the shader under the material.  We need to reference it
    // so variants will be stronger, in case we have any variants.
    auto shaderPath = _usdMaterial.GetPath().AppendChild(name);
    auto usdShader = UsdShadeShader::Define(_stage, shaderPath);
    usdShader.GetPrim().GetReferences().AddInternalReference(shaderImplPath);

    // Record the referencing shader for later variants.
    _shaders[_Name(_mtlxMaterial)][_Name(mtlxShaderNode)] =
        UsdShadeConnectableAPI(usdShader);

    // Connect to material interface.
    for (auto& i: _GetInheritanceStack(mtlxNodeDef)) {
        for (auto mtlxValue: i->getInputs()) {
            auto shaderInput   = _MakeInput(usdShader, mtlxValue);
            auto materialInput = _MakeInput(_usdMaterial, mtlxValue);
            shaderInput.ConnectToSource(materialInput);
        }
        // We deliberately ignore tokens here.
    }

    // Translate bindings.
    for (mx::InputPtr mtlxInput: mtlxShaderNode->getInputs()) {
        // Simple binding.
        _AddInputWithValue(mtlxInput, UsdShadeConnectableAPI(_usdMaterial));

        // Check if this input references an output.
        if (auto outputName = _Attr(mtlxInput, names.output)) {
            // The "nodegraph" attribute is optional.  If missing then
            // we create a USD nodegraph from the nodes and outputs on
            // the document and use that.
            mx::NodeGraphPtr mtlxNodeGraph = mtlxInput->getDocument()->
                    getNodeGraph(_Attr(mtlxInput, names.nodegraph).str());
            if (_NodeGraph usdNodeGraph =
                    mtlxNodeGraph
                        ? AddNodeGraph(mtlxNodeGraph)
                        : AddImplicitNodeGraph(mtlxInput->getDocument())) {
                _BindNodeGraph(mtlxInput,
                               _usdMaterial.GetPath(),
                               UsdShadeConnectableAPI(usdShader),
                               usdNodeGraph);
            }
        }

        // Check if this input is directly connected to (references) a node
        // Meaning the material inputs are coming from nodes not explicitly 
        // contained in a nodegraph.
        if (auto connNode = _Attr(mtlxInput, names.nodename)) {
            // Create an implicit nodegrah to contain these nodes
            if (_NodeGraph usdNodeGraph =
                        AddImplicitNodeGraph(mtlxInput->getDocument())) {
                _BindNodeGraph(mtlxInput,
                               _usdMaterial.GetPath(),
                               UsdShadeConnectableAPI(usdShader),
                               usdNodeGraph);
            }
        }
    }
    if (auto primvars = UsdGeomPrimvarsAPI(_usdMaterial)) {
        for (auto mtlxToken : mtlxShaderNode->getChildren()) {
            if (mtlxToken->getCategory() == names.token) {
                // Always use the string type for MaterialX tokens.
                auto primvar =
                    UsdGeomPrimvarsAPI(_usdMaterial)
                        .CreatePrimvar(_MakeName(mtlxToken),
                                       SdfValueTypeNames->String);
                primvar.Set(VtValue(_Attr(mtlxToken, names.value).str()));
            }
        }
    }

    // Connect the shader's outputs to the material.
    if (auto output = usdShader.GetOutput(UsdShadeTokens->surface)) {
        UsdShadeConnectableAPI::ConnectToSource(
            _usdMaterial.CreateSurfaceOutput(_tokens->mtlxRenderContext),
            output);
    }
    if (auto output = usdShader.GetOutput(UsdShadeTokens->displacement)) {
        UsdShadeConnectableAPI::ConnectToSource(
            _usdMaterial.CreateDisplacementOutput(_tokens->mtlxRenderContext),
            output);
    }
    if (auto output = usdShader.GetOutput(UsdShadeTokens->volume)) {
        UsdShadeConnectableAPI::ConnectToSource(
            _usdMaterial.CreateVolumeOutput(_tokens->mtlxRenderContext),
            output);
    }
    if (auto output = usdShader.GetOutput(_tokens->light)) {
        // USD doesn't support this type.
        UsdShadeConnectableAPI::ConnectToSource(
            _usdMaterial.CreateOutput(
                _tokens->light, SdfValueTypeNames->Token), output);
    }

    // Connect other semantic shader outputs.
    for (auto output: usdShader.GetOutputs()) {
        auto name = output.GetBaseName();
        if (name != UsdShadeTokens->surface &&
            name != UsdShadeTokens->displacement &&
            name != UsdShadeTokens->volume &&
            name != _tokens->light) {
            UsdShadeConnectableAPI::ConnectToSource(
                _usdMaterial.CreateOutput(
                    name, SdfValueTypeNames->Token), output);
        }
    }

    return usdShader;
}

void
_Context::AddMaterialVariant(
    const std::string& mtlxMaterialName,
    const VariantSetName& variantSetName,
    const VariantName& variantName) const
{
    auto mtlxMaterial = _shaders.find(mtlxMaterialName);
    if (mtlxMaterial == _shaders.end()) {
        // Unknown material.
        return;
    }
    const auto* variant = _GetVariant(variantSetName, variantName);
    if (!variant) {
        // Unknown variant.
        return;
    }

    // Create the variant set on the material.
    auto usdMaterial = GetMaterial(mtlxMaterialName);
    auto usdVariantSet = usdMaterial.GetPrim().GetVariantSet(variantSetName);

    // Create the variant on the material.
    if (!usdVariantSet.AddVariant(variantName)) {
        TF_CODING_ERROR("Failed to author material variant '%s' "
                        "in variant set '%s' on <%s>",
                        variantName.c_str(),
                        variantSetName.c_str(),
                        usdMaterial.GetPath().GetText());
        return;
    }

    usdVariantSet.SetVariantSelection(variantName);
    {
        UsdEditContext ctx(usdVariantSet.GetVariantEditContext());
        // Copy variant to the material.
        auto mtlxShaderNodeName = mtlxMaterial->second.find("");
        if (mtlxShaderNodeName != mtlxMaterial->second.end()) {
            _CopyVariant(mtlxShaderNodeName->second, *variant);
        }
    }
    usdVariantSet.ClearVariantSelection();
}

UsdCollectionAPI
_Context::AddCollection(const mx::ConstCollectionPtr& mtlxCollection)
{
    // Add the collection and any referenced collection.
    std::set<mx::ConstCollectionPtr> visited;
    return _AddCollection(mtlxCollection, &visited);
}

UsdCollectionAPI
_Context::AddGeometryReference(const mx::ConstGeomElementPtr& mtlxGeomElement)
{
    // Get the MaterialX collection.
    UsdCollectionAPI result;
    if (auto mtlxCollection = _Attr(mtlxGeomElement, names.collection)) {
        auto i = _collections.find(mtlxCollection);
        if (i != _collections.end()) {
            result = i->second;
        }
        else {
            TF_WARN("Unknown collection '%s' in %s",
                    mtlxCollection.c_str(),
                    mtlxGeomElement->getNamePath().c_str());
        }
    }

    // If there's a 'geom' attribute then use that instead.
    else if (auto collection = _AddGeomExpr(mtlxGeomElement)) {
        result = collection;
    }

    // Remember the collection for this geom element.
    return _collectionMapping[mtlxGeomElement] = result;
}

UsdCollectionAPI
_Context::_AddCollection(
    const mx::ConstCollectionPtr& mtlxCollection,
    std::set<mx::ConstCollectionPtr>* visited)
{
    if (!visited->insert(mtlxCollection).second) {
        TF_WARN("Found a collection cycle at '%s'",
                _Name(mtlxCollection).c_str());
        return UsdCollectionAPI();
    }

    // Create the prim.
    auto usdPrim = _stage->DefinePrim(_collectionsPath);

    // Create the collection.
    auto& usdCollection =
        _collections[_Name(mtlxCollection)] =
            UsdCollectionAPI::Apply(usdPrim, _MakeName(mtlxCollection));
    _SetCoreUIAttributes(usdCollection.CreateIncludesRel(), mtlxCollection);

    // Add the included collections (recursively creating them if necessary)
    // and the included and excluded geometry.
    if (auto inclcol = _Attr(mtlxCollection, names.includecollection)) {
        for (auto& collectionName: UsdMtlxSplitStringArray(inclcol.str())) {
            if (auto mtlxChildCollection =
                    mtlxCollection->getDocument()
                        ->getCollection(collectionName)) {
                if (auto usdChildCollection =
                        _AddCollection(mtlxChildCollection, visited)) {
                    usdCollection.IncludePath(
                        usdChildCollection.GetCollectionPath());
                }
            }
        }
    }
    auto& geomprefix = mtlxCollection->getActiveGeomPrefix();
    if (auto inclgeom = _Attr(mtlxCollection, names.includegeom)) {
        for (auto& path: UsdMtlxSplitStringArray(inclgeom.str())) {
            _AddGeom(usdCollection.CreateIncludesRel(), geomprefix + path);
        }
    }
    if (auto exclgeom = _Attr(mtlxCollection, names.excludegeom)) {
        for (auto& path: UsdMtlxSplitStringArray(exclgeom.str())) {
            _AddGeom(usdCollection.CreateExcludesRel(), geomprefix + path);
        }
    }
    return usdCollection;
}

UsdCollectionAPI
_Context::_AddGeomExpr(const mx::ConstGeomElementPtr& mtlxGeomElement)
{
    // Check if the 'geom' attribute exists.
    auto geom = _Attr(mtlxGeomElement, names.geom);
    if (!geom) {
        // No 'geom' attribute so give up.
        return UsdCollectionAPI();
    }

    // Since a geom attribute can only add geometry it doesn't matter
    // what order it's in.  So we split, sort, discard duplicates
    // and join to make a key.
    auto geomexprArray = UsdMtlxSplitStringArray(geom.str());
    std::sort(geomexprArray.begin(), geomexprArray.end());
    geomexprArray.erase(std::unique(geomexprArray.begin(),
                                    geomexprArray.end()),
                        geomexprArray.end());
    _GeomKey key = TfStringJoin(geomexprArray, ",");

    // See if this key exists.
    auto i = _geomSets.emplace(std::move(key), UsdCollectionAPI());
    if (!i.second) {
        // Yep, we have this collection already.
        return i.first->second;
    }

    // Nope, new collection.  Make a unique name for it.
    int& k = _nextGeomIndex;
    auto name = "geom_";
    auto usdPrim = _stage->DefinePrim(_collectionsPath);
    while (UsdCollectionAPI(usdPrim, TfToken(name + std::to_string(k)))) {
        ++k;
    }

    // Create the collection.
    auto& usdCollection =
        i.first->second =
            UsdCollectionAPI::Apply(usdPrim, TfToken(name + std::to_string(k)));

    // Add the geometry expressions.
    auto& geomprefix = mtlxGeomElement->getActiveGeomPrefix();
    for (auto& path: geomexprArray) {
        _AddGeom(usdCollection.CreateIncludesRel(), geomprefix + path);
    }

    return usdCollection;
}

void
_Context::_AddGeom(
    const UsdRelationship& rel, const std::string& pathString) const
{
    std::string errMsg;
    if (SdfPath::IsValidPathString(pathString, &errMsg)) {
        rel.AddTarget(
            SdfPath(pathString).ReplacePrefix(SdfPath::AbsoluteRootPath(),
                                              _collectionsPath));
    }
    else {
        TF_WARN("Ignored non-path '%s' on collection relationship <%s>",
                pathString.c_str(), rel.GetPath().GetText());
    }
}

const _Context::VariantSetOrder&
_Context::GetVariantSetOrder() const
{
    return _variantSetGlobalOrder;
}

UsdShadeMaterial
_Context::GetMaterial(const std::string& mtlxMaterialName) const
{
    auto i = _materials.find(mtlxMaterialName);
    return i == _materials.end() ? UsdShadeMaterial() : i->second;
}

SdfPath
_Context::GetCollectionsPath() const
{
    return _collectionsPath;
}

UsdCollectionAPI
_Context::GetCollection(
    const mx::ConstGeomElementPtr& mtlxGeomElement,
    const UsdPrim& prim) const
{
    auto i = _collectionMapping.find(mtlxGeomElement);
    if (i == _collectionMapping.end()) {
        return UsdCollectionAPI();
    }
    if (!prim) {
        return i->second;
    }

    // Remap the collection to prim.
    auto orig = i->second.GetCollectionPath();
    auto path = orig.ReplacePrefix(orig.GetPrimPath(), prim.GetPath());
    if (path.IsEmpty()) {
        return UsdCollectionAPI();
    }
    return UsdCollectionAPI::GetCollection(prim.GetStage(), path);
}

void
_Context::_BindNodeGraph(
    const mx::ConstInputPtr& mtlxInput,
    const SdfPath& referencingPathParent,
    const UsdShadeConnectableAPI& connectable,
    const _NodeGraph& usdNodeGraph)
{
    // Reference the instantiation.
    SdfPath referencingPath = referencingPathParent.AppendChild(
            usdNodeGraph.GetOwnerPrim().GetPath().GetNameToken());
    TF_DEBUG(USDMTLX_READER).Msg("_BindNodeGraph %s - %s\n",
                                 mtlxInput->getName().c_str(),
                                 referencingPath.GetString().c_str());
    _NodeGraph refNodeGraph = usdNodeGraph.AddReference(referencingPath);
    if (!refNodeGraph) {
        return;
    }

    // Connect the input to the nodegraph's output.
    const std::string &outputName = _Attr(mtlxInput, names.output);
    if (UsdShadeOutput output = refNodeGraph.GetOutputByName(outputName)) {
        UsdShadeConnectableAPI::ConnectToSource(
            _AddInput(mtlxInput, connectable),
            output);
    }
    // If this input is connected to a node's output.
    else if (auto nodename =_Attr(mtlxInput, names.nodename)) {
        // Find the conected node's UsdShadeShader node and output
        const TfToken outputToken = (outputName.empty()) 
            ? UsdMtlxTokens->DefaultOutputName
            : TfToken(outputName);
        const SdfPath shaderPath = referencingPath.AppendChild(TfToken(nodename));
        if (UsdShadeShader usdShader = UsdShadeShader::Get(
                usdNodeGraph.GetOwnerPrim().GetStage(),
                referencingPath.AppendChild(TfToken(nodename)))) {
            if (UsdShadeOutput output = usdShader.GetOutput(outputToken)) {
                UsdShadeConnectableAPI::ConnectToSource(
                    _AddInput(mtlxInput, connectable),
                    output);
            }
            else {
                TF_WARN("No output \"%s\" for input \"%s\" on <%s>",
                    outputToken.GetText(),
                    _Name(mtlxInput).c_str(),
                    shaderPath.GetText());
            }
        }
        else {
            TF_WARN("Shader not found at <%s> for input \"%s\"",
                shaderPath.GetText(),
                _Name(mtlxInput).c_str());
        }
    }
    else {
        TF_WARN("No output \"%s\" for input \"%s\" on <%s>",
                outputName.c_str(),
                _Name(mtlxInput).c_str(),
                connectable.GetPath().GetText());
    }
}

UsdShadeInput
_Context::_AddInput(
    const mx::ConstValueElementPtr& mtlxValue,
    const UsdShadeConnectableAPI& connectable)
{
    auto usdInput = _MakeInput(connectable, mtlxValue);
    _SetCoreUIAttributes(usdInput.GetAttr(), mtlxValue);
    return usdInput;
}

UsdShadeInput
_Context::_AddInputWithValue(
    const mx::ConstValueElementPtr& mtlxValue,
    const UsdShadeConnectableAPI& connectable)
{
    if (auto usdInput = _AddInput(mtlxValue, connectable)) {
        _CopyValue(usdInput, mtlxValue);
        return usdInput;
    }
    return UsdShadeInput();
}

UsdShadeOutput
_Context::_AddShaderOutput(
    const mx::ConstTypedElementPtr& mtlxTyped,
    const UsdShadeConnectableAPI& connectable)
{
    auto& type = _Type(mtlxTyped);

    std::string context;
    auto mtlxTypeDef = mtlxTyped->getDocument()->getTypeDef(type);
    if (mtlxTypeDef) {
        if (auto semantic = _Attr(mtlxTypeDef, names.semantic)) {
            if (semantic.str() == mx::SHADER_SEMANTIC) {
                context = _Attr(mtlxTypeDef, names.context);
            }
        }
    }
    TF_DEBUG(USDMTLX_READER).Msg("Add shader output %s of type %s\n",
                                 mtlxTyped->getName().c_str(),
                                 type.c_str());
    if (context == "surface" || type == mx::SURFACE_SHADER_TYPE_STRING) {
        return connectable.CreateOutput(UsdShadeTokens->surface,
                                        SdfValueTypeNames->Token);
    }
    else if (context == "displacement" || type == mx::DISPLACEMENT_SHADER_TYPE_STRING) {
        return connectable.CreateOutput(UsdShadeTokens->displacement,
                                        SdfValueTypeNames->Token);
    }
    else if (context == "volume" || type == mx::VOLUME_SHADER_TYPE_STRING) {
        return connectable.CreateOutput(UsdShadeTokens->volume,
                                        SdfValueTypeNames->Token);
    }
    else if (context == "light" || type == mx::LIGHT_SHADER_TYPE_STRING) {
        // USD doesn't support this.
        return connectable.CreateOutput(_tokens->light,
                                        SdfValueTypeNames->Token);
    }
    else if (!context.empty()) {
        // We don't know this type so use the MaterialX type name as-is.
        return connectable.CreateOutput(TfToken(type),
                                        SdfValueTypeNames->Token);
    }
    return UsdShadeOutput();
}

const _Context::Variant*
_Context::_GetVariant(
    const VariantSetName& variantSetName,
    const VariantName& variantName) const
{
    auto i = _variantSets.find(variantSetName);
    if (i != _variantSets.end()) {
        auto j = i->second.find(variantName);
        if (j != i->second.end()) {
            return &j->second;
        }
    }
    return nullptr;
}

void
_Context::_CopyVariant(
    const UsdShadeConnectableAPI& connectable,
    const Variant& variant) const
{
    for (auto& nameAndValue: variant) {
        auto& mtlxValue = nameAndValue.second;
        _CopyValue(_MakeInput(connectable, mtlxValue), mtlxValue);
    }
}

/// This class tracks variant selections on materialassigns.  Objects
/// are created using the VariantAssignmentsBuilder helper.
class VariantAssignments {
public:
    using VariantName = _Context::VariantName;
    using VariantSetName = _Context::VariantSetName;
    using VariantSetOrder = _Context::VariantSetOrder;
    using VariantSelection = std::pair<VariantSetName, VariantName>;
    using VariantSelectionSet = std::set<VariantSelection>;
    using MaterialAssignPtr = mx::ConstMaterialAssignPtr;
    using MaterialAssigns = std::vector<MaterialAssignPtr>;

    /// Add the variant assignments from \p mtlx to this object.
    void Add(const mx::ConstElementPtr& mtlx);

    /// Add the variant assignments from \p mtlxLook and all inherited
    /// looks to this object.
    void AddInherited(const mx::ConstLookPtr& mtlxLook);

    /// Compose variant assignments in this object over assignments in
    /// \p weaker and store the result in this object.
    void Compose(const VariantAssignments& weaker);

    /// Returns all material assigns.
    const MaterialAssigns& GetMaterialAssigns() const;

    /// Returns the variant set order for the material assign.
    VariantSetOrder GetVariantSetOrder(const MaterialAssignPtr&) const;

    /// Returns the variant selections on the given material assign.
    const VariantSelectionSet&
    GetVariantSelections(const MaterialAssignPtr&) const;

    using iterator = std::vector<VariantSelection>::iterator;
    iterator begin() { return _assignments.begin(); }
    iterator end() { return _assignments.end(); }

private:
    using _Assignments = std::vector<VariantSelection>;

    _Assignments _Get(const mx::ConstElementPtr& mtlx);
    void _Compose(const _Assignments& weaker);

private:
    VariantSetOrder _globalVariantSetOrder;
    MaterialAssigns _materialAssigns;
    std::map<MaterialAssignPtr, VariantSelectionSet> _selections;
    _Assignments _assignments;

    // Variant sets that have been handled already.
    std::set<VariantSetName> _seen;
    
    friend class VariantAssignmentsBuilder;
};

void
VariantAssignments::Add(const mx::ConstElementPtr& mtlx)
{
    auto&& assignments = _Get(mtlx);
    _assignments.insert(_assignments.end(),
                        std::make_move_iterator(assignments.begin()),
                        std::make_move_iterator(assignments.end()));
}

void
VariantAssignments::AddInherited(const mx::ConstLookPtr& mtlxLook)
{
    // Compose the look's variant assignments as weaker.
    _Compose(_Get(mtlxLook));

    // Compose inherited assignments as weaker.
    if (auto inherited = mtlxLook->getInheritsFrom()) {
        if (auto inheritedLook = inherited->asA<mx::Look>()) {
            AddInherited(inheritedLook);
        }
    }
}

void
VariantAssignments::Compose(const VariantAssignments& weaker)
{
    _Compose(weaker._assignments);
}

const VariantAssignments::MaterialAssigns&
VariantAssignments::GetMaterialAssigns() const
{
    return _materialAssigns;
}

VariantAssignments::VariantSetOrder
VariantAssignments::GetVariantSetOrder(
    const MaterialAssignPtr& mtlxMaterialAssign) const
{
    // We could compute and store an order per material assign instead.
    return _globalVariantSetOrder;
}

const VariantAssignments::VariantSelectionSet&
VariantAssignments::GetVariantSelections(
    const MaterialAssignPtr& mtlxMaterialAssign) const
{
    auto mtlxMaterial = _selections.find(mtlxMaterialAssign);
    if (mtlxMaterial != _selections.end()) {
        return mtlxMaterial->second;
    }
    static const VariantSelectionSet empty;
    return empty;
}

VariantAssignments::_Assignments
VariantAssignments::_Get(const mx::ConstElementPtr& mtlx)
{
    _Assignments result;

    // Last assignment wins for any given variant set.  If we wanted
    // the first to win then we wouldn't reverse.
    auto mtlxVariantAssigns = _Children(mtlx, names.variantassign);
    std::reverse(mtlxVariantAssigns.begin(), mtlxVariantAssigns.end());

    // Collect the ordered variant selections.
    for (auto& mtlxVariantAssign: mtlxVariantAssigns) {
        _Attr variantset(mtlxVariantAssign, names.variantset);
        _Attr variant(mtlxVariantAssign, names.variant);
        // Ignore assignments to a variant set we've already seen.
        if (_seen.insert(variantset).second) {
            result.emplace_back(variantset, variant);
        }
    }

    // Reverse the result since we reversed the iteration.
    std::reverse(result.begin(), result.end());

    return result;
}

void
VariantAssignments::_Compose(const _Assignments& weaker)
{
    // Apply weaker to stronger.  That means we ignore any variantsets
    // already in stronger.
    for (const auto& assignment: weaker) {
        if (_seen.insert(assignment.first).second) {
            _assignments.emplace_back(assignment);
        }
    }
}

/// Helper class to build \c VariantAssignments.
class VariantAssignmentsBuilder {
public:
    using MaterialAssignPtr = VariantAssignments::MaterialAssignPtr;

    /// Add variant assignments on a material assign to the builder.
    void Add(const MaterialAssignPtr&, VariantAssignments&&);

    /// Build and return a VariantAssignments using the added data.
    /// This also resets the builder.
    VariantAssignments Build(const _Context&);

private:
    std::map<MaterialAssignPtr, VariantAssignments> _data;
};

void
VariantAssignmentsBuilder::Add(
    const MaterialAssignPtr& mtlxMaterialAssign,
    VariantAssignments&& selection)
{
    // We don't expect duplicate keys but we use the last data added.
    _data[mtlxMaterialAssign] = std::move(selection);
}

VariantAssignments
VariantAssignmentsBuilder::Build(const _Context& context)
{
    VariantAssignments result;

    // Just tuck this away.
    result._globalVariantSetOrder = context.GetVariantSetOrder();

    // We could scan for and discard variant assignments that don't
    // affect their material here.

    // Reorganize data into result, finding variants.  A material M's
    // variants are those assigned to it over all looks.  Since each
    // variant is in a variantset this also determines the variantsets.
    //
    // We also record in the result all of the material assignments and
    // the variant info and selection for each (materialassign,variantset).
    //
    for (auto& i : _data) {
        auto& mtlxMaterialAssign  = i.first;
        auto& variantAssignments  = i.second;
        auto& selections   = result._selections[mtlxMaterialAssign];
        auto materialName  = _Attr(mtlxMaterialAssign, names.material).str();

        // Record all material assigns.
        result._materialAssigns.emplace_back(mtlxMaterialAssign);

        // Process all variants.
        for (auto& variantSelection : variantAssignments) {
            auto& variantSetName  = variantSelection.first;
            auto& variantName     = variantSelection.second;

            // Note the variant selection.
            selections.emplace(variantSetName, variantName);
        }
    }

    // Discard remaining data.
    _data.clear();

    return result;
}

// Convert MaterialX nodegraphs with nodedef attributes to UsdShadeNodeGraphs.
// This is basically a one-to-one translation of nodes to UsdShadeShaders,
// parameters and inputs to UsdShadeInputs, outputs (include default
// outputs) to UsdShadeOutputs, and input connections using the nodename
// attribute to USD connections.
static
void
ReadNodeGraphsWithDefs(mx::ConstDocumentPtr mtlx, _Context& context)
{
    // Translate nodegraphs with nodedefs.
    for (auto& mtlxNodeGraph: mtlx->getNodeGraphs()) {
        TF_DEBUG(USDMTLX_READER).Msg("Read node graph %s\n",
                                     mtlxNodeGraph->getName().c_str() );
        context.AddNodeGraphWithDef(mtlxNodeGraph);
    }
}

// Convert MaterialX nodegraphs w/out nodedef attributes to UsdShadeNodeGraphs.
// This is basically a one-to-one translation of nodes to UsdShadeShaders,
// parameters and inputs to UsdShadeInputs, outputs (include default
// outputs) to UsdShadeOutputs, and input connections using the nodename
// attribute to USD connections.
static
void
ReadNodeGraphsWithoutDefs(mx::ConstDocumentPtr mtlx, _Context& context)
{
    // Translate nodegraphs with nodedefs.
    for (auto& mtlxNodeGraph: mtlx->getNodeGraphs()) {
        if (!mtlxNodeGraph->getNodeDef()) {
            context.AddNodeGraph(mtlxNodeGraph);
        }
    }
}

// Convert MaterialX materials to USD materials.  Each USD material has
// child shader prims for each shaderNode in the MaterialX material.  In
// addition, all of the child shader inputs and outputs are connected to
// a synthesized material interface that's the union of all of those
// inputs and outputs.  The child shader prims reference shader prims
// that encapsulate the nodedef for the shader.  This necessary to
// ensure that variants opinions are stronger than the nodedef opinions
// but it also makes for a clean separation and allows sharing nodedefs
// across materials.  Material inherits are added at the end via
// specializes arcs.
// Get the associated Shader Nodes for a given MaterialX Material and translate
// them into USD equvalents
static
void 
_TranslateShaderNodes(
    _Context& context,
    const mx::NodePtr& mtlxMaterial,
    const std::string & mtlxShaderType)
{
    for (auto mtlxShaderNode: mx::getShaderNodes(mtlxMaterial, mtlxShaderType)) {
        // Translate shader node.
        TF_DEBUG(USDMTLX_READER).Msg("Adding shaderNode '%s' type: '%s'\n",
                                    _Name(mtlxShaderNode).c_str(), mtlxShaderType.c_str());
        if (auto usdShader = context.AddShaderNode(mtlxShaderNode)) {
            // Do nothing.
        }
        else {
            if (auto nodedef = _Attr(mtlxShaderNode, names.nodedef)) {
                TF_WARN("Failed to create shaderNode '%s' "
                        "to nodedef '%s'",
                        _Name(mtlxShaderNode).c_str(),
                        nodedef.c_str());
            }
            else if (auto node = _Attr(mtlxShaderNode, names.node)) {
                TF_WARN("Failed to create shaderNode '%s' "
                        "to node '%s'",
                        _Name(mtlxShaderNode).c_str(),
                        node.c_str());
            }
            else {
                // Ignore -- no node was specified.
            }
        }
    }
}

static 
void 
_TranslateShaderNodes(
    _Context& context,
    const mx::NodePtr& mtlxMaterial)
{
    _TranslateShaderNodes(context, mtlxMaterial, mx::SURFACE_SHADER_TYPE_STRING);
    _TranslateShaderNodes(context, mtlxMaterial, mx::VOLUME_SHADER_TYPE_STRING);
    _TranslateShaderNodes(context, mtlxMaterial, mx::DISPLACEMENT_SHADER_TYPE_STRING);
    _TranslateShaderNodes(context, mtlxMaterial, mx::LIGHT_SHADER_TYPE_STRING);
}

static
void
ReadMaterials(mx::ConstDocumentPtr mtlx, _Context& context)
{
    for (auto& mtlxMaterial: mtlx->getMaterialNodes()) {
        // Translate material.
        TF_DEBUG(USDMTLX_READER).Msg("Adding mtlxMaterial '%s'\n",
                                     _Name(mtlxMaterial).c_str());
        if (auto usdMaterial = context.BeginMaterial(mtlxMaterial)) {
            // Translate all shader nodes.
            _TranslateShaderNodes(context, mtlxMaterial);
            context.EndMaterial();
        }
        else {
            TF_WARN("Failed to create material '%s'",
                    _Name(mtlxMaterial).c_str());
        }
    }

    // Add material inherits.  We wait until now so we can be sure all
    // the materials exist.
    for (auto& mtlxMaterial: mtlx->getMaterialNodes()) {
        if (auto usdMaterial = context.GetMaterial(_Name(mtlxMaterial))) {
            if (auto name = _Attr(mtlxMaterial, names.inherit)) {
                if (auto usdInherited = context.GetMaterial(name)) {
                    usdMaterial.GetPrim().GetSpecializes()
                        .AddSpecialize(usdInherited.GetPath());
                    TF_DEBUG(USDMTLX_READER).Msg("Material '%s' inherit from "
                                                 " '%s'\n",
                                                 _Name(mtlxMaterial).c_str(),
                                                 name.c_str());
                }
                else {
                    TF_WARN("Material '%s' attempted to inherit from "
                            "unknown material '%s'",
                            _Name(mtlxMaterial).c_str(), name.c_str());
                }
            }
        }
    }
}

// Convert MaterialX collections and geom attributes on material assigns
// to USD collections.  All collections go onto a single prim in USD.
// All paths are absolutized and MaterialX paths that require geomexpr
// are discarded with a warning (since USD only supports simple absolute
// paths in collections).  geom attributes are converted to collections
// because USD material binding requires a UsdCollectionAPI.  geomprefix
// is baked into the paths during this conversion.  Equal collections
// are shared;  we note the source MaterialX element and the resulting
// USD collection here so we can bind it later.
static
bool
ReadCollections(mx::ConstDocumentPtr mtlx, _Context& context)
{
    bool hasAny = false;

    // Translate all collections.
    for (auto& mtlxCollection: mtlx->getCollections()) {
        context.AddCollection(mtlxCollection);
        hasAny = true;
    }

    // Make a note of the geometry on each material assignment.
    for (auto& mtlxLook: mtlx->getLooks()) {
        for (auto& mtlxMaterialAssign: mtlxLook->getMaterialAssigns()) {
            context.AddGeometryReference(mtlxMaterialAssign);
        }
    }

    return hasAny;
}

// Creates the variants bound to a MaterialX materialassign on the USD
// Material and/or its shader children.  The variant opinions go on the
// Material bound to the materialassign. 
static
void
AddMaterialVariants(
    const mx::ConstMaterialAssignPtr& mtlxMaterialAssign,
    const _Context& context,
    const VariantAssignments& assignments)
{
    std::string materialName = _Attr(mtlxMaterialAssign, names.material);

    // Process variant sets in the appropriate order.
    for (const auto& variantSetName:
            assignments.GetVariantSetOrder(mtlxMaterialAssign)) {
        // Loop over all variants in the variant set on the material.
        for (const auto& variantSelections :
                assignments.GetVariantSelections(mtlxMaterialAssign)) {
            // Add the variant to the material.
            context.AddMaterialVariant(materialName, variantSetName,
                                       variantSelections.second);
        }
    }
}

// Converts a MaterialX look to a USD prim.  This prim references the
// collections so it can use them in any material binding.  It has a
// UsdShadeMaterialBindingAPI and a Material child prim under a
// "Materials" scope for each materialassign.  The Material prims
// will use variant selections for each MaterialX variantassign and
// will reference the materials created by ReadMaterials().
//
// If the look has a inherit then the USD will reference the corresponding
// USD prim.
static
void
ReadLook(
    const mx::ConstLookPtr& mtlxLook,
    const UsdPrim& root,
    _Context& context,
    const VariantAssignments& assignments,
    bool hasCollections)
{
    static const TfToken materials("Materials");

    _SetCoreUIAttributes(root, mtlxLook);

    // Add a reference for the inherit, if any.
    if (auto inherit = _Attr(mtlxLook, names.inherit)) {
        auto path =
            root.GetPath().GetParentPath().AppendChild(_MakeName(inherit));
        root.GetReferences().AddInternalReference(path);
    }

    // Add a reference to the collections in each look so they can use
    // them in bindings.  Inheriting looks will get the collections
    // directly and via the inherited look.  USD will collapse these
    // into a single reference.
    if (hasCollections) {
        root.GetReferences().AddInternalReference(
            context.GetCollectionsPath());
    }

    // Make a prim for all of the materials.
    auto lookMaterialsPrim =
        root.GetStage()->DefinePrim(root.GetPath().AppendChild(materials));

    // Collect all of the material assign names and whether the name
    // has been used yet.
    std::map<TfToken, int> materialNames;
    for (auto& mtlxMaterialAssign: mtlxLook->getMaterialAssigns()) {
        materialNames[_MakeName(mtlxMaterialAssign)] = 0;
    }
    for (auto&& child: lookMaterialsPrim.GetAllChildren()) {
        // Inherited.
        materialNames[child.GetName()] = 1;
    }

    // Make an object for binding materials.
    auto binding = UsdShadeMaterialBindingAPI::Apply(root);

    // Get the current (inherited) property order.
    const auto inheritedOrder = root.GetPropertyOrder();

    // Add each material assign and record the order of material bindings.
    TfTokenVector order;
    for (auto& mtlxMaterialAssign: mtlxLook->getMaterialAssigns()) {
        // Get the USD material.
        auto usdMaterial =
            context.GetMaterial(_Attr(mtlxMaterialAssign, names.material));
        if (!usdMaterial) {
            // Unknown material.
            continue;
        }

        // Make a unique material name.  If possible use the name of
        // the materialassign.
        auto materialName = _MakeName(mtlxMaterialAssign);
        int& n = materialNames[materialName];
        if (n) {
            // Make a unique name.
            auto stage   = lookMaterialsPrim.GetStage();
            auto base    = lookMaterialsPrim.GetPath();
            auto prefix  = materialName.GetString() + "_";
            do {
                materialName = TfToken(prefix + std::to_string(n++));
            } while (stage->GetPrimAtPath(base.AppendChild(materialName)));
        }
        else {
            // We've used the name now.
            n = 1;
        }

        // Make a material prim.  This has the MaterialX name of the
        // material assign since we can assign the same material
        // multiple times with different variants to different
        // collections (so we can't use the material name itself).
        auto lookMaterialPrim =
            lookMaterialsPrim.GetStage()->DefinePrim(
                lookMaterialsPrim.GetPath().AppendChild(materialName));
        _SetGlobalCoreUIAttributes(lookMaterialPrim, mtlxMaterialAssign);

        // Reference the original material.
        lookMaterialPrim.GetReferences()
            .AddInternalReference(usdMaterial.GetPath());

        // Set the variant selections.
        for (const auto& i:
                assignments.GetVariantSelections(mtlxMaterialAssign)) {
            lookMaterialPrim.GetVariantSet(i.first)
                .SetVariantSelection(i.second);
        }

        // Find the collection.
        if (auto collection =
                context.GetCollection(mtlxMaterialAssign, root)) {
            // Bind material to a collection.
            if (binding.Bind(collection, UsdShadeMaterial(lookMaterialPrim),
                             materialName)) {
                // Record the binding.
                order.push_back(
                    binding.GetCollectionBindingRel(materialName).GetName());
            }
        }
        else {
            // Bind material to the prim.
            if (binding.Bind(UsdShadeMaterial(lookMaterialPrim))) {
                // Record the binding.
                order.push_back(binding.GetDirectBindingRel().GetName());
            }
        }
    }

    // Ensure our local material bindings are strongest and in the
    // right order.
    if (!order.empty()) {
        order.insert(order.end(),
                     inheritedOrder.begin(), inheritedOrder.end());
        root.SetPropertyOrder(order);
    }
}

} // anonymous namespace

void
UsdMtlxRead(
    const MaterialX::ConstDocumentPtr& mtlxDoc,
    const UsdStagePtr& stage,
    const SdfPath& internalPath,
    const SdfPath& externalPath)
{
    if (!mtlxDoc) {
        TF_CODING_ERROR("Invalid MaterialX document");
        return;
    }
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return;
    }
    if (!internalPath.IsPrimPath()) {
        TF_CODING_ERROR("Invalid internal prim path");
        return;
    }
    if (!externalPath.IsPrimPath()) {
        TF_CODING_ERROR("Invalid external prim path");
        return;
    }

    _Context context(stage, internalPath);

    // Color management.
    if (auto cms = _Attr(mtlxDoc, names.cms)) {
        stage->SetColorManagementSystem(TfToken(cms));
    }
    if (auto cmsconfig = _Attr(mtlxDoc, names.cmsconfig)) {
        // XXX -- Is it okay to use the URI as is?
        stage->SetColorConfiguration(SdfAssetPath(cmsconfig));
    }
    auto&& colorspace = mtlxDoc->getActiveColorSpace();
    if (!colorspace.empty()) {
        // XXX This information will be lost because layer metadata does not 
        // currently compose across a reference.
        VtDictionary dict;
        dict[SdfFieldKeys->ColorSpace.GetString()] = VtValue(colorspace);
        stage->SetMetadata(SdfFieldKeys->CustomLayerData, dict);
    }

    // Read in locally defined Custom Nodes defined with a nodegraph.
    ReadNodeGraphsWithDefs(mtlxDoc, context);

    // Translate all materials.
    ReadMaterials(mtlxDoc, context);

    // If there are no looks then we're done.
    if (mtlxDoc->getLooks().empty()) {
        return;
    }

    // Collect the MaterialX variants.
    context.AddVariants(mtlxDoc);

    // Translate all collections.
    auto hasCollections = ReadCollections(mtlxDoc, context);

    // Collect all of the material/variant assignments.
    VariantAssignmentsBuilder materialVariantAssignmentsBuilder;
    for (auto& mtlxLook : mtlxDoc->getLooks()) {
        // Get the variant assigns for the look and (recursively) its
        // inherited looks.
        VariantAssignments lookVariantAssigns;
        lookVariantAssigns.AddInherited(mtlxLook);

        for (auto& mtlxMaterialAssign: mtlxLook->getMaterialAssigns()) {
            // Get the material assign's variant assigns.
            VariantAssignments variantAssigns;
            variantAssigns.Add(mtlxMaterialAssign);

            // Compose variantAssigns over lookVariantAssigns.
            variantAssigns.Compose(lookVariantAssigns);

            // Note all of the assigned variants.
            materialVariantAssignmentsBuilder
                .Add(mtlxMaterialAssign, std::move(variantAssigns));
        }
    }

    // Build the variant assignments object.
    auto assignments = materialVariantAssignmentsBuilder.Build(context);

    // Create the variants on each material.
    for (const auto& mtlxMaterialAssign : assignments.GetMaterialAssigns()) {
        AddMaterialVariants(mtlxMaterialAssign, context, assignments);
    }

    // Make an internal path for looks.
    auto looksPath = internalPath.AppendChild(TfToken("Looks"));

    // Create the external root prim.
    auto root = stage->DefinePrim(externalPath);

    // Create each look as a variant.
    auto lookVariantSet = root.GetVariantSets().AddVariantSet("LookVariant");
    for (auto& mtlxMostDerivedLook : mtlxDoc->getLooks()) {
        // We rely on inherited looks to exist in USD so we do
        // those first.
        for (auto& mtlxLook : _GetInheritanceStack(mtlxMostDerivedLook)) {
            auto lookName = _Name(mtlxLook);

            // Add the look prim.  If it already exists (because it was
            // inherited by a previously handled look) then skip it.
            auto usdLook =
                stage->DefinePrim(looksPath.AppendChild(TfToken(lookName)));
            if (usdLook.HasAuthoredReferences()) {
                continue;
            }

            // Read the look.
            ReadLook(mtlxLook, usdLook, context, assignments, hasCollections);

            // Create a variant for this look in the external root.
            if (lookVariantSet.AddVariant(lookName)) {
                lookVariantSet.SetVariantSelection(lookName);
                UsdEditContext ctx(lookVariantSet.GetVariantEditContext());
                root.GetReferences().AddInternalReference(usdLook.GetPath());
            }
            else {
                TF_CODING_ERROR("Failed to author look variant '%s' "
                                "in variant set '%s' on <%s>",
                                lookName.c_str(),
                                lookVariantSet.GetName().c_str(),
                                root.GetPath().GetText());
            }
        }
    }
    lookVariantSet.ClearVariantSelection();
}

void
UsdMtlxReadNodeGraphs(
    const MaterialX::ConstDocumentPtr& mtlxDoc,
    const UsdStagePtr& stage,
    const SdfPath& internalPath)
{
    if (!mtlxDoc) {
        TF_CODING_ERROR("Invalid MaterialX document");
        return;
    }
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return;
    }
    if (!internalPath.IsPrimPath()) {
        TF_CODING_ERROR("Invalid internal prim path");
        return;
    }

    _Context context(stage, internalPath);

    ReadNodeGraphsWithDefs(mtlxDoc, context);
    ReadNodeGraphsWithoutDefs(mtlxDoc, context);
}

PXR_NAMESPACE_CLOSE_SCOPE
