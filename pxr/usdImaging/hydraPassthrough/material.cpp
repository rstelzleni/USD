#include "pxr/usdImaging/hydraPassthrough/material.h"
#include "pxr/usdImaging/hydraPassthrough/renderParam.h"

#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hio/glslfx.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (UsdPreviewSurface)         \
    (diffuseColor)              \
    (opacity)                   \
    (opacityMode)               \
    (opacityThreshold)          \
    (sourceColorSpace)          \
    ((colorSpaceAuto, "auto"))  \
    (masked)                    \
    (translucent)               \
    (transparent)               \
    (defaultMaterialTag)        \
    (swizzle)                   \
    (isPtex)                    \
    (a)                         \
    (st)                        \
    (uv)                        \
    (in)                        \
    (rotation)                  \
    (scale)                     \
    (translation)               \
    (storm)                     \
    (bias)                      \
    (textureMemory)             \
    (fieldname)
);

namespace {

static HdMaterialNode2 const*
_GetTerminalNode(
    HdMaterialNetwork2 const& network,
    TfToken const& terminalName,
    SdfPath * terminalNodePath)
{
    // Get the terminal from this network, could be a Surface or Volume Terminal
    auto const& terminalConnIt = network.terminals.find(terminalName);
    if (terminalConnIt == network.terminals.end()) {
        return nullptr;
    }
    HdMaterialConnection2 const& connection = terminalConnIt->second;
    SdfPath const& terminalPath = connection.upstreamNode;
    auto const& terminalIt = network.nodes.find(terminalPath);
    *terminalNodePath = terminalPath;
    return &terminalIt->second;
}

// Similar to the version in hdSt/materialNetwork.cpp but doesn't allocate
// GL resources.
static void
_GetGlslfxForTerminal(
    HioGlslfxSharedPtr& glslfxOut,
    size_t *glslfxOutHash,
    TfToken const& nodeTypeId)
{
    HD_TRACE_FUNCTION();

    // If there is a URI, we will use that, otherwise we will try to use
    // the source code.
    SdrRegistry &shaderReg = SdrRegistry::GetInstance();
    SdrShaderNodeConstPtr sdrNode = shaderReg.GetShaderNodeByIdentifierAndType(
        nodeTypeId, HioGlslfxTokens->glslfx);

    if (sdrNode) {
        std::string const& glslfxFilePath = sdrNode->GetResolvedImplementationURI();
        if (!glslfxFilePath.empty()) {

            // Hash the filepath if it has changed.
            if (!(*glslfxOutHash) ||
                (glslfxOut && glslfxOut->GetFilePath() != glslfxFilePath)) {
                *glslfxOutHash = TfHash()(glslfxFilePath);
            }

            // XXX RYANS
            // This reads the glslfx file and parses it into some data structures.
            // It will often be the same glslfx file, so we should probably be
            // caching these.
            glslfxOut = std::make_shared<HioGlslfx>(glslfxFilePath);
        } else {
            std::string const& sourceCode = sdrNode->GetSourceCode();
            if (!sourceCode.empty()) {
                // Do not use the registry for the source code to avoid
                // the cost of hashing the entire source code.
                std::istringstream sourceCodeStream(sourceCode);
                glslfxOut = std::make_shared<HioGlslfx>(sourceCodeStream);
            }
        }
    }
}

// In HdSt this function uses some Storm specific tokens to report things like,
// is this a foreground/hud object, is it masked like tree leaves, is it
// translucent, etc. We'll do our best to report that to the caller. It's
// kind of unfortunate that this logic is locked up in HdSt and we have to
// duplicate it here.
static TfToken
_GetMaterialTag(
    VtDictionary const& metadata,
    HdMaterialNode2 const& terminal)
{
    // Strongest materialTag opinion is a hardcoded tag in glslfx meta data.
    // This can be used for masked, additive, translucent or volume materials.
    // See HdMaterialTagTokens (deprecated in favor of HdStMaterialTagTokens).
    VtValue vtMetaTag = TfMapLookupByValue(
        metadata,
        HdShaderTokens->materialTag,
        VtValue());

    if (vtMetaTag.IsHolding<std::string>()) {
        return TfToken(vtMetaTag.UncheckedGet<std::string>());
    }

    // Next check for authored terminal.opacityThreshold value > 0
    for (auto const& paramIt : terminal.parameters) {
        if (paramIt.first != _tokens->opacityThreshold) continue;

        VtValue const& vtOpacityThreshold = paramIt.second;
        if (vtOpacityThreshold.Get<float>() > 0.0f) {
            return _tokens->masked;
        }
    }

    bool isTranslucent = false;

    // Next strongest opinion is a connection to 'terminal.opacity'
    auto const& opacityConIt = terminal.inputConnections.find(_tokens->opacity);
    isTranslucent = (opacityConIt != terminal.inputConnections.end());

    // Weakest opinion is an authored terminal.opacity value.
    if (!isTranslucent) {
        for (auto const& paramIt : terminal.parameters) {
            if (paramIt.first != _tokens->opacity) continue;

            VtValue const& vtOpacity = paramIt.second;
            isTranslucent = vtOpacity.Get<float>() < 1.0f;
            break;
        }
    }

    if (isTranslucent) {
        return _tokens->translucent;
    }

    // An empty materialTag on the HdRprimCollection level means: 'ignore all
    // materialTags and add everything to the collection'. Instead we return a
    // default token because we want materialTags to drive HdSt collections.
    return _tokens->defaultMaterialTag;
}

static TfToken
_GetPrimvarNameAttributeValue(
    SdrShaderNodeConstPtr const& sdrNode,
    HdMaterialNode2 const& node,
    TfToken const& propName)
{
    VtValue vtName;
    auto const& paramIt = node.parameters.find(propName);
    if (paramIt != node.parameters.end()) {
        vtName = paramIt->second;
    }

    // If we didn't find an authored value consult Sdr for the default value.
    if (vtName.IsEmpty() && sdrNode) {
        if (SdrShaderPropertyConstPtr sdrPrimvarInput = 
                sdrNode->GetShaderInput(propName)) {
            vtName = sdrPrimvarInput->GetDefaultValue();
        }
    }

    if (vtName.IsHolding<TfToken>()) {
        return vtName.UncheckedGet<TfToken>();
    } else if (vtName.IsHolding<std::string>()) {
        return TfToken(vtName.UncheckedGet<std::string>());
    }

    return TfToken();
}

static VtValue
_GetNodeFallbackValue(
    HdMaterialNode2 const& node,
    TfToken const& outputName)
{
    SdrRegistry &shaderReg = SdrRegistry::GetInstance();

    // Find the corresponding Sdr node.
    SdrShaderNodeConstPtr const sdrNode = 
        shaderReg.GetShaderNodeByIdentifierAndType(node.nodeTypeId,
                                                   HioGlslfxTokens->glslfx);
    if (!sdrNode) {
        return VtValue();
    }

    // XXX Storm hack: Incorrect usage of GetDefaultInput to
    // determine what the fallback value is.
    // GetDefaultInput is meant to be used for 'disabled'
    // node where the 'default input' becomes the value
    // pass-through in the network. But Storm has no other
    // mechanism currently to deal with fallback values.
    //
    // I'm copying this hack for compatibility
    if (SdrShaderPropertyConstPtr const& defaultInput = 
            sdrNode->GetDefaultInput()) {
        TfToken const& def = defaultInput->GetName();
        auto const& defParamIt = node.parameters.find(def);
        if (defParamIt != node.parameters.end()) {
            return defParamIt->second;
        }
    }

    // Sdr supports specifying default values for outputs so if we
    // did not use the GetDefaultInput hack above, we fallback to
    // using this DefaultOutput value.
    if (SdrShaderPropertyConstPtr const& output = 
            sdrNode->GetShaderOutput(outputName)) {
        const VtValue out =  output->GetDefaultValue();
        if (!out.IsEmpty()) {
            return out;
        }

        // If no default value was registered with Sdr for
        // the output, fallback to the type's default.
        return output->GetTypeAsSdfType().GetSdfType().GetDefaultValue();
    }

    return VtValue();
}

static VtValue
_GetParamFallbackValue(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    TfToken const& paramName)
{
    // The 'fallback value' will be the value of the material param if nothing 
    // is connected or what is connected is mis-configured. For example a 
    // missing texture file.

    SdrRegistry& shaderReg = SdrRegistry::GetInstance();

    // Check if there are any connections to the terminal input.
    auto const& connIt = node.inputConnections.find(paramName);

    if (connIt != node.inputConnections.end()) {
        if (!connIt->second.empty()) {
            HdMaterialConnection2 const& con = connIt->second.front();
            auto const& pnIt = network.nodes.find(con.upstreamNode);
            HdMaterialNode2 const& upstreamNode = pnIt->second;
        
            const VtValue fallbackValue =
                _GetNodeFallbackValue(upstreamNode, con.upstreamOutputName);
            if (!fallbackValue.IsEmpty()) {
                return fallbackValue;
            }
        }
    }

    // If there are no connections there may be an authored value.

    auto const& it = node.parameters.find(paramName);
    if (it != node.parameters.end()) {
        return it->second;
    }

    // If we had nothing connected, but we do have an Sdr node we can use the
    // DefaultValue for the input as specified in the Sdr schema.
    // E.g. PreviewSurface is a terminal with an Sdr schema.

    SdrShaderNodeConstPtr terminalSdr = 
        shaderReg.GetShaderNodeByIdentifierAndType(
            node.nodeTypeId,
            HioGlslfxTokens->glslfx);

    if (terminalSdr) {
        if (SdrShaderPropertyConstPtr const& input = 
                terminalSdr->GetShaderInput(paramName)) {
            VtValue out = input->GetDefaultValue();
            // If not default value was registered with Sdr for
            // the output, fallback to the type's default.
            if (out.IsEmpty()) {
                out = input->GetTypeAsSdfType().GetSdfType().GetDefaultValue();
            }

            if (!out.IsEmpty()) return out;
        }
    }

    // Returning an empty value will likely result in a shader compile error,
    // because the buffer source will not be able to determine the HdTupleType.
    // Hope for the best and return a vec3.

    TF_WARN("Couldn't determine default value for: %s on nodeType: %s", 
            paramName.GetText(), node.nodeTypeId.GetText());

    return VtValue(GfVec3f(0));
}

static std::string
_ResolveAssetPath(VtValue const& value)
{
    // XXX RYANS
    // Note that the SdfAssetPath should really be resolved into an ArAsset via
    // ArGetResolver (Eg. USDZ). Using GetResolvePath directly isn't sufficient.
    // Texture loading in Storm goes via Glf, which will handle the ArAsset
    // resolution already, so HdSt relies on that. We might need to use the resolver,
    // but it isn't clear at the moment what we'd want to present to the user
    // for USDZ support. Probably we extract and host the images...
    if (value.IsHolding<SdfAssetPath>()) {
        SdfAssetPath p = value.Get<SdfAssetPath>();
        std::string v = p.GetResolvedPath();
        if (v.empty()) {
            v = p.GetAssetPath();
        }
        return v;
    } else if (value.IsHolding<std::string>()) {
        return value.UncheckedGet<std::string>();
    }

    return std::string();
}

template<typename T>
static auto
_ResolveParameter(
    HdMaterialNode2 const& node,
    SdrShaderNodeConstPtr const &sdrNode,
    TfToken const &name,
    T const &defaultValue) -> T
{
    // First consult node parameters...
    const auto it = node.parameters.find(name);
    if (it != node.parameters.end()) {
        const VtValue &value = it->second;
        if (value.IsHolding<T>()) {
            return value.UncheckedGet<T>();
        }
    }

    // Then fallback to SdrNode.
    if (sdrNode) {
        if (SdrShaderPropertyConstPtr const input =
                                        sdrNode->GetShaderInput(name)) {
            const VtValue &value = input->GetDefaultValue();
            if (value.IsHolding<T>()) {
                return value.UncheckedGet<T>();
            }
        }
    }

    return defaultValue;
}

static void
_MakeMaterialParamsForUnconnectedParam(
    TfToken const& paramName,
    std::vector<HydraPassthroughMaterialParam> *params)
{
    HydraPassthroughMaterialParam param;
    param.paramType = HydraPassthroughMaterialParam::ParamType::Fallback;
    param.name = paramName;

    params->push_back(std::move(param));
}

static void
_MakeMaterialParamsForAdditionalPrimvar(
    TfToken const& primvarName,
    std::vector<HydraPassthroughMaterialParam> *params)
{
    HydraPassthroughMaterialParam param;
    param.paramType = HydraPassthroughMaterialParam::ParamType::AdditionalPrimvar;
    param.name = primvarName;

    params->push_back(std::move(param));
}

static void
_MakeMaterialParamsForPrimvarReader(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    SdfPath const& nodePath,
    TfToken const& paramName,
    SdfPathSet* visitedNodes,
    std::vector<HydraPassthroughMaterialParam> *params)
{
    if (visitedNodes->find(nodePath) != visitedNodes->end()) return;

    SdrRegistry& shaderReg = SdrRegistry::GetInstance();
    SdrShaderNodeConstPtr sdrNode = shaderReg.GetShaderNodeByIdentifierAndType(
        node.nodeTypeId, HioGlslfxTokens->glslfx);

    HydraPassthroughMaterialParam param;
    param.paramType = HydraPassthroughMaterialParam::ParamType::PrimvarRedirect;
    param.name = paramName;

    // A node may require 'additional primvars' to function correctly.
    for (auto const& propName: sdrNode->GetAdditionalPrimvarProperties()) {
        TfToken primvarName = 
            _GetPrimvarNameAttributeValue(sdrNode, node, propName);

        if (!primvarName.IsEmpty()) {
            param.samplerCoords.push_back(primvarName);
        }
    }

    params->push_back(std::move(param));
}

static void
_MakeMaterialParamsForFieldReader(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    SdfPath const& nodePath,
    TfToken const& paramName,
    SdfPathSet* visitedNodes,
    std::vector<HydraPassthroughMaterialParam> *params)
{
    if (visitedNodes->find(nodePath) != visitedNodes->end()) return;

    // Volume Fields act more like a primvar then a texture.
    // There is a `Volume` prim with 'fields' that may point to a
    // OpenVDB file. We have to find the 'inputs:fieldname' on the
    // HWFieldReader in the material network to know what 'field' to use.
    // See also HdStVolume and HdStField for how volume textures are
    // inserted into Storm.

    HydraPassthroughMaterialParam param;
    param.paramType = HydraPassthroughMaterialParam::ParamType::FieldRedirect;
    param.name = paramName;

    // XXX Why _tokens->fieldname:
    // Hard-coding the name of the attribute of HwFieldReader identifying
    // the field name for now.
    // The equivalent of the generic mechanism Sdr provides for primvars
    // is missing for fields: UsdPrimvarReader.inputs:varname is tagged with
    // sdrMetadata as primvarProperty="1" so that we can use
    // sdrNode->GetAdditionalPrimvarProperties to know what attribute to use.
    TfToken const& varName = _tokens->fieldname;

    auto const& it = node.parameters.find(varName);
    if (it != node.parameters.end()){
        VtValue fieldName = it->second;
        if (fieldName.IsHolding<TfToken>()) {
            // Stashing name of field in _samplerCoords.
            param.samplerCoords.push_back(
                fieldName.UncheckedGet<TfToken>());
        } else if (fieldName.IsHolding<std::string>()) {
            param.samplerCoords.push_back(
                TfToken(fieldName.UncheckedGet<std::string>()));
        }
    }

    params->push_back(std::move(param));
}

static void
_MakeMaterialParamsForTransform2d(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    SdfPath const& nodePath,
    TfToken const& paramName,
    SdfPathSet* visitedNodes,
    std::vector<HydraPassthroughMaterialParam> *params)
{
    if (visitedNodes->find(nodePath) != visitedNodes->end()) return;

    SdrRegistry& shaderReg = SdrRegistry::GetInstance();

    HydraPassthroughMaterialParam transform2dParam;
    transform2dParam.paramType = HydraPassthroughMaterialParam::ParamType::Transform2d;
    transform2dParam.name = paramName;
    transform2dParam.fallbackValue = _GetParamFallbackValue(network, node,
                                                            _tokens->in);

    std::vector<HydraPassthroughMaterialParam> additionalParams;

    // Find the input connection to the transform2d node
    auto inIt = node.inputConnections.find(_tokens->in);
    if (inIt != node.inputConnections.end()) {
        if (!inIt->second.empty()) {
            HdMaterialConnection2 const& con = inIt->second.front();
            SdfPath const& upstreamNodePath = con.upstreamNode;
            
            auto const& pnIt = network.nodes.find(upstreamNodePath);
            HdMaterialNode2 const& primvarNode = pnIt->second;
            SdrShaderNodeConstPtr primvarSdr = 
                shaderReg.GetShaderNodeByIdentifierAndType(
                    primvarNode.nodeTypeId, HioGlslfxTokens->glslfx);

            if (primvarSdr) {
                std::vector<HydraPassthroughMaterialParam> primvarParams;

                _MakeMaterialParamsForPrimvarReader(
                    network,
                    primvarNode,
                    upstreamNodePath,
                    inIt->first,
                    visitedNodes,
                    &primvarParams);

                if (!primvarParams.empty()) {
                    HydraPassthroughMaterialParam const& primvarParam = 
                        primvarParams.front();
                    // Extract the referenced primvar(s) to go into the
                    // transform2d's sampler coords.
                    transform2dParam.samplerCoords = primvarParam.samplerCoords;
                }

                // Make sure we add any referenced primvars as "additional
                // primvars" so they make it through primvar filtering.
                //
                // XXX RYANS Does this matter for my implementation?
                for (auto const& primvarName : transform2dParam.samplerCoords) {
                    _MakeMaterialParamsForAdditionalPrimvar(
                        primvarName, &additionalParams);
                }
            }
        }
    } else {
        // See if input value was directly authored as value.
        auto iter = node.parameters.find(_tokens->in);

        if (iter != node.parameters.end()) {
            if (iter->second.IsHolding<TfToken>()) {
                TfToken const& samplerCoord = 
                    iter->second.UncheckedGet<TfToken>();
                transform2dParam.samplerCoords.push_back(samplerCoord);
            }
        }
    }

    params->push_back(std::move(transform2dParam));

    // Make materials params for each component of transform2d
    // (rotation, scale, translation)
    HydraPassthroughMaterialParam rotParam;
    rotParam.paramType = HydraPassthroughMaterialParam::ParamType::Fallback;
    rotParam.name = TfToken(paramName.GetString() + "_" + 
                            _tokens->rotation.GetString());
    rotParam.fallbackValue = _GetParamFallbackValue(network, node,
                                                    _tokens->rotation);
    params->push_back(std::move(rotParam));

    HydraPassthroughMaterialParam scaleParam;
    scaleParam.paramType = HydraPassthroughMaterialParam::ParamType::Fallback;
    scaleParam.name = TfToken(paramName.GetString() + "_" + 
                              _tokens->scale.GetString());
    scaleParam.fallbackValue = _GetParamFallbackValue(network, node,
                                                      _tokens->scale);
    params->push_back(std::move(scaleParam));

    HydraPassthroughMaterialParam transParam;
    transParam.paramType = HydraPassthroughMaterialParam::ParamType::Fallback;
    transParam.name = TfToken(paramName.GetString() + "_" + 
                              _tokens->translation.GetString());
    transParam.fallbackValue = _GetParamFallbackValue(network, node,
                                                      _tokens->translation);
    params->push_back(std::move(transParam));

    // Need to add these at the end because the caller expects the
    // "transform" param to be first.
    params->insert(params->end(),
            additionalParams.begin(),
            additionalParams.end());
}

static void
_MakeMaterialParamsForTexture(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    HdMaterialNode2 const& downstreamNode, // needed to determine def value
    SdfPath const& nodePath,
    TfToken const& outputName,
    TfToken const& paramName,
    SdfPathSet* visitedNodes,
    std::vector<HydraPassthroughMaterialParam> *params,
    std::vector<HydraPassthroughMaterial::TextureDescriptor> *textureDescriptors,
    TfToken const& materialTag)
{
    if (visitedNodes->find(nodePath) != visitedNodes->end()) return;

    SdrRegistry& shaderReg = SdrRegistry::GetInstance();
    SdrShaderNodeConstPtr sdrNode = shaderReg.GetShaderNodeByIdentifier(
        node.nodeTypeId, {HioGlslfxTokens->glslfx}); //, _tokens->mtlx}); for MaterialX

    HydraPassthroughMaterialParam texParam;
    texParam.paramType = HydraPassthroughMaterialParam::ParamType::Texture;
    texParam.name = paramName;

    // Get swizzle metadata if possible
    if (SdrShaderPropertyConstPtr sdrProperty = sdrNode->GetShaderOutput(outputName)) {
        SdrTokenMap const& propMetadata = sdrProperty->GetMetadata();
        auto const& it = propMetadata.find(_tokens->swizzle);
        if (it != propMetadata.end()) {
            texParam.swizzle = it->second;
        }
    }

    // Determine the texture type
    texParam.textureType = HydraPassthroughMaterialParam::TextureType::Uv;
    if (sdrNode && sdrNode->GetMetadata().count(_tokens->isPtex)) {
        texParam.textureType = HydraPassthroughMaterialParam::TextureType::Ptex;
    }

    // Determine if texture should be pre-multiplied on CPU
    // Currently, this will only happen if the texture param is called 
    // "diffuseColor" and if there is another param "opacity" connected to the
    // same texture node via output "a", as long as the material tag is not 
    // "masked"
    //
    // I'm not sure what "currently" means in this context. If this changes in
    // HdSt or upstream in Hd we'd need to update this too.
    bool premultiplyTexture = false;
    if (paramName == _tokens->diffuseColor && 
        materialTag != _tokens->masked) {
        auto const& opacityConIt = downstreamNode.inputConnections.find(
            _tokens->opacity);
        if (opacityConIt != downstreamNode.inputConnections.end()) {
            HdMaterialConnection2 const& con = opacityConIt->second.front();
            premultiplyTexture = ((nodePath == con.upstreamNode) && 
                                  (con.upstreamOutputName == _tokens->a));
        } 
    }
    texParam.isPremultiplied = premultiplyTexture;

    // Get texture's sourceColorSpace hint 
    //
    // Should be a token, but also check for string to support older files
    TfToken sourceColorSpace = _ResolveParameter(
        node, sdrNode, _tokens->sourceColorSpace, TfToken());
    if (sourceColorSpace.IsEmpty()) {
        const std::string sourceColorSpaceStr = _ResolveParameter(
            node, sdrNode, _tokens->sourceColorSpace, 
            _tokens->colorSpaceAuto.GetString());
        sourceColorSpace = TfToken(sourceColorSpaceStr);
    }

    // Extract texture file path
    // Note that HdSt uses a TextureIdentifier type that can also identify
    // a "subtexture" which may be like a frame in a video, or a slice of
    // a volume. I'm not supporting that yet because I don't know if clients
    // could use it.
    TfToken textureFilePath;

    SdrTokenVec const& assetIdentifierPropertyNames = 
        sdrNode->GetAssetIdentifierInputNames();

    if (!assetIdentifierPropertyNames.empty()) {
        TfToken fileProp = assetIdentifierPropertyNames[0];

        // Some MaterialX nodes can have multiple file inputs. Take the first
        // one that matches the param name. If we lookup a <trilinear> texture
        // against an output named "N42_fileY", we will find the right one.
        //
        // Not supporting MaterialX yet, but when we do, do this
        //if (assetIdentifierPropertyNames.size() > 1) {
        //    for (auto const& propName: assetIdentifierPropertyNames) {
        //        if (TfStringEndsWith(outputName.GetString(), propName)) {
        //            fileProp = propName;
        //            break;
        //        }
        //    }
        //}

        auto const& it = node.parameters.find(fileProp);
        if (it != node.parameters.end()){
            const VtValue &v = it->second;
            // The type of the filepath attribute could also be SdfPath
            // which means it's a path to a prim that holds the texture.
            // We're not handling that, but it could enable things like
            // render-to-texture.
            if (v.IsHolding<std::string>() ||
                v.IsHolding<SdfAssetPath>()) {
                const std::string filePath = _ResolveAssetPath(v);

                bool isUdim = TfStringContains(filePath, "<UDIM>");
                if (isUdim) {
                    texParam.textureType = HydraPassthroughMaterialParam::TextureType::Udim;
                }

                textureFilePath = TfToken(filePath);

                // If we support subtextures, this is the place to extract
                // them.
            } 
         }
    } else {
        TF_WARN("Invalid number of asset identifier input names: %s", 
                nodePath.GetText());
    }

    // Check to see if a primvar or transform2d node is connected to 'st' or 
    // 'uv'.
    // Instead of looking for a st inputs by name we could traverse all
    // connections to inputs and pick one that has a 'primvar' or 'transform2d' 
    // node attached. That could also be problematic if you connect a primvar or 
    // transform2d to one of the other inputs of the texture node.
    //
    // XXX RYANS
    // Couldn't this also look for connections of type texcoord2d?
    auto stIt = node.inputConnections.find(_tokens->st);
    if (stIt == node.inputConnections.end()) {
        stIt = node.inputConnections.find(_tokens->uv);
    }

    // We have uvs
    if (stIt != node.inputConnections.end()) {
        if (!stIt->second.empty()) {
            HdMaterialConnection2 const& con = stIt->second.front();
            SdfPath const& upstreamNodePath = con.upstreamNode;
            
            auto const& upIt = network.nodes.find(upstreamNodePath);
            HdMaterialNode2 const& upstreamNode = upIt->second;

            SdrShaderNodeConstPtr upstreamSdr = 
                shaderReg.GetShaderNodeByIdentifierAndType(
                    upstreamNode.nodeTypeId, HioGlslfxTokens->glslfx);

            if (upstreamSdr) {
                TfToken sdrRole(upstreamSdr->GetRole());
                if (sdrRole == SdrNodeRole->Primvar) {
                    std::vector<HydraPassthroughMaterialParam> primvarParams;

                    _MakeMaterialParamsForPrimvarReader(
                        network,
                        upstreamNode,
                        upstreamNodePath,
                        stIt->first,
                        visitedNodes,
                        &primvarParams);

                    if (!primvarParams.empty()) {
                        HydraPassthroughMaterialParam const& primvarParam = primvarParams.front();
                        // Extract the referenced primvar(s) for use in the texture
                        // sampler coords.
                        texParam.samplerCoords = primvarParam.samplerCoords;
                    }

                    // For any referenced primvars, add them as "additional primvars"
                    for (auto const& primvarName : texParam.samplerCoords) {
                        _MakeMaterialParamsForAdditionalPrimvar(
                            primvarName, params);
                    }
                } else if (sdrRole == SdrNodeRole->Math) {
                    std::vector<HydraPassthroughMaterialParam> transform2dParams;

                    _MakeMaterialParamsForTransform2d(
                        network,
                        upstreamNode,
                        upstreamNodePath,
                        TfToken(paramName.GetString() + "_" + 
                                stIt->first.GetString() + "_transform2d"),
                        visitedNodes,
                        &transform2dParams);

                     if (!transform2dParams.empty()) {
                        HydraPassthroughMaterialParam const& transform2dParam = 
                            transform2dParams.front();
                        // The texure's sampler coords should come from the
                        // output of the transform2d
                        texParam.samplerCoords.push_back(transform2dParam.name);
                    }

                    // Copy params created for tranform2d node to param list
                    params->insert(params->end(), 
                                   transform2dParams.begin(), 
                                   transform2dParams.end());
                }
            }
        }
    } else {

        // See if a st value was directly authored as value.
        
        auto iter = node.parameters.find(_tokens->st);
        if (iter == node.parameters.end()) {
            iter = node.parameters.find(_tokens->uv);
        }

        if (iter != node.parameters.end()) {
            if (iter->second.IsHolding<TfToken>()) {
                TfToken const& samplerCoord = 
                    iter->second.UncheckedGet<TfToken>();
                    texParam.samplerCoords.push_back(samplerCoord);
            }
        }
    }

    // Handle texture scale and bias
    //
    // XXX RYANS this uses parameter names like textureName_storm_scale which
    // seems weird. Are these authored this way in the source data?
    HydraPassthroughMaterialParam texScaleParam;
    texScaleParam.paramType = HydraPassthroughMaterialParam::ParamType::Fallback;
    texScaleParam.name = TfToken(paramName.GetString() + "_" +
                                 _tokens->storm.GetString() + "_" +
                                 _tokens->scale.GetString());
    texScaleParam.fallbackValue = VtValue(_ResolveParameter(node, 
                                                            sdrNode, 
                                                            _tokens->scale, 
                                                            GfVec4f(1.0f)));
    params->push_back(std::move(texScaleParam));

    HydraPassthroughMaterialParam texBiasParam;
    texBiasParam.paramType = HydraPassthroughMaterialParam::ParamType::Fallback;
    texBiasParam.name = TfToken(paramName.GetString() + "_" +
                                _tokens->storm.GetString() + "_" +
                                _tokens->bias.GetString());
    texBiasParam.fallbackValue = VtValue(_ResolveParameter(node, 
                                                           sdrNode, 
                                                           _tokens->bias, 
                                                           GfVec4f(0.0f)));
    params->push_back(std::move(texBiasParam));

    // Attribute is in Mebibytes, but lets convert to bytes
    const size_t memoryRequest = 1048576 * 
        _ResolveParameter<float>(node, sdrNode, _tokens->textureMemory, 0.0f);

    textureDescriptors->push_back(
        { paramName,
          textureFilePath,
          texParam.textureType,
          HdGetSamplerParameters(node, sdrNode, nodePath),
          memoryRequest });

    params->push_back(std::move(texParam));
}

static void
_MakeParamsForInputParameter(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    TfToken const& paramName,
    SdfPathSet* visitedNodes,
    std::vector<HydraPassthroughMaterialParam> *params,
    std::vector<HydraPassthroughMaterial::TextureDescriptor> *textureDescriptors,
    TfToken const& materialTag)
{
    SdrRegistry& shaderReg = SdrRegistry::GetInstance();

    // Resolve what is connected to this param (eg. primvar, texture, nothing)
    // and then make the correct MaterialParam for it.
    auto const& conIt = node.inputConnections.find(paramName);

    if (conIt != node.inputConnections.end()) {

        std::vector<HdMaterialConnection2> const& cons = conIt->second;
        if (!cons.empty()) {

            // Find the node that is connected to this input
            HdMaterialConnection2 const& con = cons.front();
            auto const& upIt = network.nodes.find(con.upstreamNode);

            if (upIt != network.nodes.end()) {

                SdfPath const& upstreamPath = upIt->first;
                TfToken const& upstreamOutputName = con.upstreamOutputName;
                HdMaterialNode2 const& upstreamNode = upIt->second;

                SdrShaderNodeConstPtr upstreamSdr = 
                    shaderReg.GetShaderNodeByIdentifier(
                        upstreamNode.nodeTypeId,
                        {HioGlslfxTokens->glslfx}); //, _tokens->mtlx}); to also get MaterialX

                if (upstreamSdr) {
                    TfToken sdrRole(upstreamSdr->GetRole());
                    if (sdrRole == SdrNodeRole->Texture) {
                        _MakeMaterialParamsForTexture(
                            network,
                            upstreamNode,
                            node,
                            upstreamPath,
                            upstreamOutputName,
                            paramName,
                            visitedNodes,
                            params,
                            textureDescriptors,
                            materialTag);
                        return;
                    } else if (sdrRole == SdrNodeRole->Primvar) {
                        _MakeMaterialParamsForPrimvarReader(
                            network,
                            upstreamNode,
                            upstreamPath,
                            paramName,
                            visitedNodes,
                            params);
                        return;
                    } else if (sdrRole == SdrNodeRole->Field) {
                        // XXX RYANS
                        // Not really supporting volumes yet, but it might not
                        // hurt to add the parameters if they exist.
                        _MakeMaterialParamsForFieldReader(
                            network,
                            upstreamNode,
                            upstreamPath,
                            paramName,
                            visitedNodes,
                            params);
                        return;
                    } else if (sdrRole == SdrNodeRole->Math) {
                        _MakeMaterialParamsForTransform2d(
                            network,
                            upstreamNode,
                            upstreamPath,
                            paramName,
                            visitedNodes,
                            params);
                        return;
                    }
                } else {
                    TF_WARN("Unrecognized connected node: %s", 
                            upstreamNode.nodeTypeId.GetText());
                }
            }
        }
    } 

    // Nothing (supported) was connected, output a fallback material param    
    _MakeMaterialParamsForUnconnectedParam(paramName, params);
}

static void
_GatherMaterialParams(
    HdMaterialNetwork2 const& network,
    HdMaterialNode2 const& node,
    std::vector<HydraPassthroughMaterialParam> *params,
    std::vector<HydraPassthroughMaterial::TextureDescriptor> *textureDescriptors,
    TfToken const& materialTag)
{
    HD_TRACE_FUNCTION();

    // We only support PreviewSurfaces at the moment. The following code will
    // handle input values authored directly or connected to a primvar, or
    // connected to a texture. Textures may also have a connected primvar
    // to provide UVs. Volume connections are not supported (yet), but will
    // still be added to the params list.
    //
    // This should be sufficient for PreviewSurface, but may not be sufficient
    // for arbitrary materials like glslfx or MaterialX might support. More
    // complex networks may be connected to inputs in those cases, so might
    // require a more robust network traversal.

    SdrRegistry &shaderReg = SdrRegistry::GetInstance();
    SdrShaderNodeConstPtr const sdrNode =
        shaderReg.GetShaderNodeByIdentifierAndType(
            node.nodeTypeId, HioGlslfxTokens->glslfx);

    if (sdrNode) {
        SdfPathSet visitedNodes;
        for (TfToken const& inputName : sdrNode->GetShaderInputNames()) {
            _MakeParamsForInputParameter(
                network, node, inputName, &visitedNodes,
                params, textureDescriptors, materialTag);
        }
    } else {
        TF_WARN("Unrecognized node: %s", node.nodeTypeId.GetText());
    }

    // Set fallback values for the inputs on the terminal (excepting
    // referenced sampler coords).
    for (auto& p : *params) {
        if (p.paramType != HydraPassthroughMaterialParam::ParamType::AdditionalPrimvar &&
            p.fallbackValue.IsEmpty()) {
            p.fallbackValue = _GetParamFallbackValue(network, node, p.name);
            // The opacityMode input on a PreviewSurface material is a token
            // input, this needs to be updated to an int VtValue for codegen.
            // The values are updated such that transparent = 1 and presence = 0. 
            if (node.nodeTypeId == _tokens->UsdPreviewSurface &&
                p.name == _tokens->opacityMode) {
                int paramInt = 1;
                if (p.fallbackValue.IsHolding<std::string>()) {
                    const std::string param = p.fallbackValue.Get<std::string>();
                    paramInt = param == _tokens->transparent;
                }
                else if (p.fallbackValue.IsHolding<TfToken>()) {
                    const TfToken param = p.fallbackValue.Get<TfToken>();
                    paramInt = param == _tokens->transparent;
                }
                p.fallbackValue = VtValue(paramInt);
            }
        }
    }

    if (sdrNode) {
        // Create MaterialParams for each primvar the terminal says it needs.
        //
        // Primvars come from 'attributes' in the glslfx and are seperate from
        // the input 'parameters'. We need to create a material param for them
        // so that these primvars survive 'primvar filtering' that discards any
        // unused primvars on the mesh.
        //
        // XXX RYANS again, does this primvar filtering happen in my case?
        //
        // If the network lists additional primvars, we add those too.
        SdrTokenVec pv = sdrNode->GetPrimvars();
        pv.insert(pv.end(), network.primvars.begin(), network.primvars.end());
        std::sort(pv.begin(), pv.end());
        pv.erase(std::unique(pv.begin(), pv.end()), pv.end());

        for (TfToken const& primvarName : pv) {
            _MakeMaterialParamsForAdditionalPrimvar(primvarName, params);
        }
    }
}

} // anonymous namespace

HydraPassthroughMaterial::HydraPassthroughMaterial(SdfPath const& id)
    : HdMaterial(id) {
    TF_STATUS("Creating HydraPassthroughMaterial with id=%s", id.GetText());
}

HydraPassthroughMaterial::~HydraPassthroughMaterial() {
    TF_STATUS("Destroying HydraPassthroughMaterial with id=%s", GetId().GetText());
}

HdDirtyBits
HydraPassthroughMaterial::GetInitialDirtyBitsMask() const
{
    return AllDirty;
}


bool
HydraPassthroughMaterial::_ProcessMaterialNetwork(
    HdSceneDelegate *sceneDelegate)
{
    // This function is similar to HdStMaterialNetwork::ProcessMaterialNetwork
    // That version allocates gpu resources so we can't use it (or copy it)
    // directly.
    // It is a good reference for future improvements, such as:
    // - support for materialX compiled glsl shaders
    // - volume shaders
    // This version currently only supports UsdPreviewSurfaces

    // Get the material network from the scene delegate.
    VtValue vtMat = sceneDelegate->GetMaterialResource(GetId());
    if (vtMat.IsHolding<HdMaterialNetworkMap>()) {
        HdMaterialNetworkMap const& hdNetworkMap =
            vtMat.UncheckedGet<HdMaterialNetworkMap>();

        if (!hdNetworkMap.terminals.empty() && !hdNetworkMap.map.empty()) {
            // The fragment source comes from the 'surface' network or the
            // 'volume' network.
            bool isVolume = false;

            // Use HdMaterialNetwork2 because HdMaterialNetwork seems deprecated.
            HdMaterialNetwork2 surfaceNetwork =
                HdConvertToHdMaterialNetwork2(hdNetworkMap, &isVolume);

            if (isVolume) {
                // We don't support volume materials
                _isVolumeMaterial = true;
                return false;
            }
            const TfToken &terminalName = HdMaterialTerminalTokens->surface;

            SdfPath surfTerminalPath;
            if (HdMaterialNode2 const* surfTerminal = 
                    _GetTerminalNode(surfaceNetwork,
                                     terminalName,
                                     &surfTerminalPath)) {

                _isPreviewSurface =
                    (surfTerminal->nodeTypeId == _tokens->UsdPreviewSurface);

// When we enable support for MaterialX, we'll need this code
#ifdef PXR_MATERIALX_SUPPORT_ENABLED
//            if (!isVolume) {
//                _materialXGfx = HdSt_ApplyMaterialXFilter(&surfaceNetwork, materialId,
//                                      *surfTerminal, surfTerminalPath,
//                                      &_materialParams, resourceRegistry);
//            }
#endif
                // At this point Storm extracts the glslfx file for the terminal
                // being used. In our case we're assuming UsdPreviewSurface, and
                // we're not shipping the glslfx code for preview surface. If we
                // want to support MaterialX compiled shaders, or better preview
                // surface fidelity we could also get the glsl code and return it
                // for the browser to compile. This is the place to get that.

                // Extract the glslfx and metadata for surface
                _GetGlslfxForTerminal(_surfaceGfx, &_surfaceGfxHash,
                                      surfTerminal->nodeTypeId);
                if (_surfaceGfx && _surfaceGfx->IsValid()) {

                    //_fragmentSource = _surfaceGfx->GetSurfaceSource();
                    //_volumeSource = _surfaceGfx->GetVolumeSource();

                    _materialMetadata = _surfaceGfx->GetMetadata();
                    _materialTag = _GetMaterialTag(_materialMetadata, *surfTerminal);
                    _GatherMaterialParams(surfaceNetwork, *surfTerminal,
                            &_materialParams, &_textureDescriptors, 
                            _materialTag);

                    // OSL networks have a displacement network in hdNetworkMap
                    // under terminal: HdMaterialTerminalTokens->displacement.
                    // Storm expects the displacement shader to be provided via
                    // the surface glslfx / terminal.
                    //
                    // We'll use whatever the surface gives us
                    _displacementSource = _surfaceGfx->GetDisplacementSource();
                }
            }
        }
    }

    return true;
}

void
HydraPassthroughMaterial::Sync(HdSceneDelegate *sceneDelegate,
                               HdRenderParam   *renderParam,
                               HdDirtyBits     *dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;

    if (!(bits & DirtyResource) && !(bits & DirtyParams)) {
        *dirtyBits = Clean;
        return;
    }

    if (_ProcessMaterialNetwork(sceneDelegate)) {
        TF_STATUS("HydraPassthroughMaterial::Sync: "
                  "Processed material network for id=%s",
                  GetId().GetText());
    }

        

        // Got this far
        // These are members, unless I split this out into a processor class
        /*
        if (!hdNetworkMap.terminals.empty() && !hdNetworkMap.map.empty()) {
            _networkProcessor.ProcessMaterialNetwork(GetId(), hdNetworkMap,
                                                    resourceRegistry.get());
            fragmentSource = _networkProcessor.GetFragmentCode();
            volumeSource = _networkProcessor.GetVolumeCode();
            displacementSource = _networkProcessor.GetDisplacementCode();
            materialMetadata = _networkProcessor.GetMetadata();
            materialTag = _networkProcessor.GetMaterialTag();
            params = _networkProcessor.GetMaterialParams();
                textureDescriptors = _networkProcessor.GetTextureDescriptors();
        }
    }
    */
    //
    // Update material parameters
    //
    /*
    _materialNetworkShader->SetParams(params);

    HdBufferSpecVector specs;
    HdBufferSourceSharedPtrVector sources;

    bool hasPtex = false;
    for (auto const & param: params) {
        if (param.IsPrimvarRedirect() || param.IsFallback() || 
            param.IsTransform2d()) {
            HdSt_MaterialNetworkShader::AddFallbackValueToSpecsAndSources(
                param, &specs, &sources);
        } else if (param.IsTexture()) {
            HdSt_MaterialNetworkShader::AddFallbackValueToSpecsAndSources(
                param, &specs, &sources);
            if (param.textureType == HydraPassthroughMaterialParam::TextureType::Ptex) {
                hasPtex = true;
            }
        }
    }
    */

    // If we ever support continuous rendering, we'll need to dirty rprims when a
    // material changes. To do that we'd call the commented code below. For now 
    // we set up and do a first render to extract info, then shut down the render
    // engine.
    //    HdChangeTracker& changeTracker =
    //                     sceneDelegate->GetRenderIndex().GetChangeTracker();
    //    changeTracker.MarkAllRprimsDirty(HdChangeTracker::DirtyMaterialId);

    _AddMaterialToOutput(renderParam);

    // These pipes are clean
    *dirtyBits = Clean;
}

void
HydraPassthroughMaterial::_AddMaterialToOutput(
    HdRenderParam   *renderParam)
{
    auto rp = dynamic_cast<HydraPassthroughRenderParam*>(renderParam);
    if (!rp) {
        TF_CODING_ERROR("HydraPassthroughMesh::Sync: "
                  "renderParam is not a HydraPassthroughRenderParam, "
                  "cannot proceed.");
        return;
    }
    HydraPassthroughRenderDataRefPtr renderData = rp->GetRenderData();

    HydraPassthroughRenderData::MaterialData matData;

    matData.id = GetId();

    matData.type =
        _isPreviewSurface ?
        HydraPassthroughRenderData::MaterialData::MaterialType::PreviewSurface :
        _isVolumeMaterial ?
        HydraPassthroughRenderData::MaterialData::MaterialType::Volume :
        HydraPassthroughRenderData::MaterialData::MaterialType::Other;
    matData.tag = _materialTag;

    renderData->AddMaterial(matData.id, matData);
}

PXR_NAMESPACE_CLOSE_SCOPE
