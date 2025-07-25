//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"
#include "pxr/usd/usd/prim.h"

#include "pxr/usd/usd/debugCodes.h"
#include "pxr/usd/usd/errors.h"
#include "pxr/usd/usd/instanceCache.h"
#include "pxr/usd/usd/resolver.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/tokens.h"
#include "pxr/usd/usd/primRange.h"

#include "pxr/usd/kind/registry.h"

#include "pxr/base/tf/exception.h"
#include "pxr/base/tf/stringUtils.h"

#include <algorithm>
#include <sstream>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

// Static assertion on PrimData size.  We want to be warned when its size
// changes.
static_assert(sizeof(Usd_PrimData) == 64,
              "Expected sizeof(Usd_PrimData) == 64");

// Usd_PrimData need to be always initialized with a valid type info pointer
static const UsdPrimTypeInfo *_GetEmptyPrimTypeInfo() 
{
    static const UsdPrimTypeInfo *empty = &UsdPrimTypeInfo::GetEmptyPrimType();
    return empty;
}

Usd_PrimData::Usd_PrimData(UsdStage *stage, const SdfPath& path)
    : _stage(stage)
    , _primIndex(nullptr)
    , _path(path)
    , _primTypeInfo(_GetEmptyPrimTypeInfo())
    , _firstChild(nullptr)
    , _refCount(0)
{
    if (!stage)
        TF_FATAL_ERROR("Attempted to construct with null stage");

    TF_DEBUG(USD_PRIM_LIFETIMES).Msg(
        "Usd_PrimData::ctor<%s,%s,%s>\n",
        GetTypeName().GetText(), path.GetText(),
        _stage->GetRootLayer()->GetIdentifier().c_str());
}

Usd_PrimData::~Usd_PrimData() {
    TF_DEBUG(USD_PRIM_LIFETIMES).Msg(
        "~Usd_PrimData::dtor<%s,%s,%s>\n",
        GetTypeName().GetText(), _path.GetText(),
        _stage ? _stage->GetRootLayer()->GetIdentifier().c_str() :
        "prim is invalid/expired");
}

Usd_PrimDataConstPtr
Usd_PrimData::GetParent() const
{
    if (Usd_PrimDataPtr parentLink = GetParentLink())
        return parentLink;

    SdfPath parent = _path.GetParentPath();
    return parent == SdfPath::EmptyPath() ?
        nullptr : _stage->_GetPrimDataAtPath(parent);
}

const PcpPrimIndex &
Usd_PrimData::GetPrimIndex() const
{
    static const PcpPrimIndex dummyPrimIndex;
    return ARCH_UNLIKELY(IsPrototype()) ? dummyPrimIndex : *_primIndex;
}

const PcpPrimIndex &
Usd_PrimData::GetSourcePrimIndex() const
{
    TF_AXIOM(_primIndex);
    return *_primIndex;
}

SdfSpecifier
Usd_PrimData::GetSpecifier() const
{
    return UsdStage::_GetSpecifier(this);
}

void
Usd_PrimData::_ComposeAndCacheFlags(Usd_PrimDataConstPtr parent, 
                                    bool isPrototypePrim)
{
    // We do not have to clear _flags here since in the pseudo root or instance
    // prototype case the values never change, and in the ordinary prim case we
    // set every flag (with the exception of the pseudo root flag which is only
    // set true for the pseudo root and always remains false for every other
    // prim)

    // Special-case the root (the only prim which has no parent) and
    // instancing prototypes.
    if (ARCH_UNLIKELY(!parent || isPrototypePrim)) {
        _flags[Usd_PrimActiveFlag] = true;
        _flags[Usd_PrimLoadedFlag] = true;
        _flags[Usd_PrimModelFlag] = true;
        _flags[Usd_PrimGroupFlag] = true;
        _flags[Usd_PrimComponentFlag] = false;
        _flags[Usd_PrimDefinedFlag] = true;
        _flags[Usd_PrimHasDefiningSpecifierFlag] = true;
        _flags[Usd_PrimPrototypeFlag] = isPrototypePrim;
        _flags[Usd_PrimPseudoRootFlag] = !parent;
    } 
    else {
        // Compose and cache 'active'.
        const bool active = UsdStage::_IsActive(this);
        _flags[Usd_PrimActiveFlag] = active;

        // Cache whether or not this prim has a payload.
        bool hasPayload = _primIndex->HasAnyPayloads();
        _flags[Usd_PrimHasPayloadFlag] = hasPayload;

        // An active prim is loaded if it's loadable and in the load set, or
        // it's not loadable and its parent is loaded.
        _flags[Usd_PrimLoadedFlag] = active &&
            (hasPayload ?
             _stage->_GetPcpCache()->IsPayloadIncluded(_primIndex->GetPath()) :
             parent->IsLoaded());

        // According to Model hierarchy rules, only Model Groups may have Model
        // children (groups or otherwise).  So if our parent is not a Model
        // Group, then this prim cannot be a model (or a model group).
        // Otherwise we look up the kind metadata and consult the kind registry.
        bool isGroup = false, isModel = false, isComponent = false;
        if (parent->IsGroup()) {
            const TfToken kind = UsdStage::_GetKind(this);
            // Use the kind registry to determine model/groupness.
            if (!kind.IsEmpty()) {
                isGroup = KindRegistry::IsGroup(kind);
                isComponent = KindRegistry::IsComponent(kind);
                isModel = isGroup || isComponent || KindRegistry::IsModel(kind);
            }
        }
        _flags[Usd_PrimGroupFlag] = isGroup;
        _flags[Usd_PrimModelFlag] = isModel;
        _flags[Usd_PrimComponentFlag] = isComponent;

        // Get specifier.
        const SdfSpecifier specifier = GetSpecifier();

        // This prim is abstract if its parent is or if it's a class.
        _flags[Usd_PrimAbstractFlag] =
            parent->IsAbstract() || specifier == SdfSpecifierClass;

        // Cache whether or not this prim has an authored defining specifier.
        const bool isDefiningSpec = SdfIsDefiningSpecifier(specifier);
        _flags[Usd_PrimHasDefiningSpecifierFlag] = isDefiningSpec;

        // This prim is defined if its parent is and its specifier is defining.
        _flags[Usd_PrimDefinedFlag] = isDefiningSpec && parent->IsDefined();

        // The presence of clips that may affect attributes on this prim
        // is computed and set in UsdStage. Default to false.
        _flags[Usd_PrimClipsFlag] = false;

        // These flags indicate whether this prim is an instance or an
        // instance prototype.
        _flags[Usd_PrimInstanceFlag] = active && _primIndex->IsInstanceable();
        _flags[Usd_PrimPrototypeFlag] = parent->IsInPrototype();
    }
}

bool
Usd_PrimData::IsSubComponent() const
{
    const TfToken kind = UsdStage::_GetKind(this);
    return KindRegistry::IsSubComponent(kind);
}

Usd_PrimDataConstPtr 
Usd_PrimData::GetPrimDataAtPathOrInPrototype(const SdfPath &path) const
{
    return _stage->_GetPrimDataAtPathOrInPrototype(path);
}

Usd_PrimDataConstPtr 
Usd_PrimData::GetPrototype() const
{
    return _stage->_GetPrototypeForInstance(this);
}

bool
Usd_PrimData::_ComposePrimChildNames(TfTokenVector* nameOrder)
{
    // TODO: would be nice to not compute the name order until it is needed
    // TODO: What do we do with prohibitedNames?
    PcpTokenSet prohibitedNames;
    GetSourcePrimIndex().ComputePrimChildNames(nameOrder, &prohibitedNames);
    return true;
}

std::string
Usd_DescribePrimData(const Usd_PrimData *p, SdfPath const &proxyPrimPath)
{
    if (!p)
        return "null prim";

    const bool isDead = p->_IsDead();
    const bool isInstance = p->IsInstance();
    const bool isInstanceProxy = Usd_IsInstanceProxy(p, proxyPrimPath);
    const bool isInPrototype = isInstanceProxy ?
        Usd_InstanceCache::IsPathInPrototype(proxyPrimPath) : 
        p->IsInPrototype();
    const bool isPrototype = p->IsPrototype();
    const Usd_PrimDataConstPtr prototypeForInstance =
        isInstance && p->_stage ? p->GetPrototype() : nullptr;

    std::string desc = TfStringPrintf(
        "%s%s%sprim %s<%s> ",
        isDead ? "expired " : (p->_flags[Usd_PrimActiveFlag] ?
                                      "" : "inactive "),
        p->GetTypeName().IsEmpty() ? "" :
            TfStringPrintf("'%s' ", p->GetTypeName().GetText()).c_str(),
        // XXX: Add applied schemas to this descriptor
        isInstance ? "instance " : isInstanceProxy ? "instance proxy " : "",
        isInPrototype ? "in prototype " : "",
        isInstanceProxy ? proxyPrimPath.GetText() : p->_path.GetText());

    if (!isDead) {
        if (isInstanceProxy || isInstance) {
            if (isInstance && !prototypeForInstance) {
                desc += "with expired prototype";
            }
            else {
                desc += TfStringPrintf(
                    "with prototype <%s> ", isInstance ?
                    prototypeForInstance->GetPath().GetText() :
                    p->_path.GetText());
            }
        }
            
        if (isInstanceProxy || isPrototype || isInPrototype) {
            desc += TfStringPrintf(
                "using prim index <%s> ",
                p->GetSourcePrimIndex().GetPath().GetText());
        }

        desc += TfStringPrintf(
            "on %s", UsdDescribe(p->_stage).c_str()).c_str();
    }

    return desc;
}

void
Usd_ThrowExpiredPrimAccessError(const Usd_PrimData *p)
{
    PXR_TF_THROW(UsdExpiredPrimAccessError,
             TfStringPrintf(
                 "Used %s", Usd_DescribePrimData(p, SdfPath()).c_str()));
}


PXR_NAMESPACE_CLOSE_SCOPE

