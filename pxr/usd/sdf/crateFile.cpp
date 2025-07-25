//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#include "pxr/pxr.h"
#include "crateFile.h"
#include "integerCoding.h"

#include "pxr/base/arch/demangle.h"
#include "pxr/base/arch/errno.h"
#include "pxr/base/arch/fileSystem.h"
#include "pxr/base/arch/regex.h"
#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/arch/virtualMemory.h"
#include "pxr/base/gf/half.h"
#include "pxr/base/gf/matrix2d.h"
#include "pxr/base/gf/matrix3d.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/quath.h"
#include "pxr/base/gf/traits.h"
#include "pxr/base/gf/vec2d.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec2h.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/gf/vec4h.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/errorMark.h"
#include "pxr/base/tf/exception.h"
#include "pxr/base/tf/fastCompression.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/hash.h"
#include "pxr/base/tf/mallocTag.h"
#include "pxr/base/tf/ostreamMethods.h"
#include "pxr/base/tf/pxrTslRobinMap/robin_set.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/safeOutputFile.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/ts/binary.h"
#include "pxr/base/ts/spline.h"
#include "pxr/base/trace/trace.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/base/vt/value.h"
#include "pxr/base/work/dispatcher.h"
#include "pxr/base/work/singularTask.h"
#include "pxr/base/work/utils.h"
#include "pxr/base/work/withScopedParallelism.h"
#include "pxr/usd/ar/asset.h"
#include "pxr/usd/ar/resolvedPath.h"
#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/layerOffset.h"
#include "pxr/usd/sdf/listOp.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/pathExpression.h"
#include "pxr/usd/sdf/pathTable.h"
#include "pxr/usd/sdf/payload.h"
#include "pxr/usd/sdf/reference.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/sdf/types.h"

#include <tbb/concurrent_queue.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>

PXR_NAMESPACE_OPEN_SCOPE

#ifdef PXR_PREFER_SAFETY_OVER_SPEED
static constexpr bool SafetyOverSpeed = true;
#else
static constexpr bool SafetyOverSpeed = false;
#endif

static inline unsigned int
_GetPageShift(unsigned int mask)
{
    unsigned int shift = 1;
    mask = ~mask;
    while (mask >>= 1) {
        ++shift;
    }
    return shift;
}

static const unsigned int CRATE_PAGESIZE = ArchGetPageSize();
static const uint64_t CRATE_PAGEMASK =
    ~(static_cast<uint64_t>(CRATE_PAGESIZE-1));
static const unsigned int CRATE_PAGESHIFT = _GetPageShift(CRATE_PAGEMASK);

TF_REGISTRY_FUNCTION(TfType) {
    TfType::Define<Sdf_CrateFile::TimeSamples>();
}

#define DEFAULT_NEW_VERSION "0.8.0"
TF_DEFINE_ENV_SETTING(
    USD_WRITE_NEW_USDC_FILES_AS_VERSION, DEFAULT_NEW_VERSION,
    "When writing new Sdf Crate files, write them as this version.  "
    "This must have the same major version as the software and have less or "
    "equal minor and patch versions.  This is only for new files; saving "
    "edits to an existing file preserves its version.");

TF_DEFINE_ENV_SETTING(
    USDC_MMAP_PREFETCH_KB, 0,
    "If set to a nonzero value, attempt to disable the OS's prefetching "
    "behavior for memory-mapped files and instead do simple aligned block "
    "fetches of the given size instead.  If necessary the setting value is "
    "rounded up to the next whole multiple of the system's page size "
    "(typically 4 KB).");

TF_DEFINE_ENV_SETTING(
    USDC_ENABLE_ZERO_COPY_ARRAYS, true,
    "Enable the zero-copy optimization for numeric array values whose in-file "
    "representation matches the in-memory representation.  With this "
    "optimization, we create VtArrays that point directly into the memory "
    "mapped region rather than copying the data to heap buffers.");

TF_DEFINE_ENV_SETTING(
    USDC_USE_ASSET, false,
    "If set, data for Crate files will be read using ArAsset::Read. Crate "
    "will not use system I/O functions like mmap or pread directly for Crate "
    "files on disk, but these functions may be used indirectly by ArAsset "
    "implementations.");

static int _GetMMapPrefetchKB()
{
    auto getKB = []() {
        int setting = TfGetEnvSetting(USDC_MMAP_PREFETCH_KB);
        int kb =
            ((setting * 1024 + CRATE_PAGESIZE - 1) & CRATE_PAGEMASK) / 1024;
        if (setting != kb) {
            fprintf(stderr, "Rounded USDC_MMAP_PREFETCH_KB value %d to %d",
                    setting, kb);
        }
        return kb;
    };
    static int kb = getKB();
    return kb;
}

// Write nbytes bytes to asset at pos.
static inline int64_t
WriteToAsset(ArWritableAsset* asset,
             void const *bytes, int64_t nbytes, int64_t pos)
{
    TfErrorMark m;

    int64_t nwritten = asset->Write(bytes, nbytes, pos);
    if (ARCH_UNLIKELY(nwritten != nbytes)) {
        // Aggregate error messages into a single runtime error for brevity
        std::string errMsg;
        if (!m.IsClean()) {
            std::vector<std::string> errs;
            for (const TfError& e : m) {
                errs.push_back(e.GetCommentary());
            }
            errMsg = ": ";
            errMsg += TfStringJoin(errs, "; ");
        }

        TF_RUNTIME_ERROR("Failed writing usdc data%s", errMsg.c_str());
        nwritten = 0;
    }
    return nwritten;
}

/// \class SdfReadOutOfBoundsError
///
/// Sdf throws this exception when code attempts to read
/// memory outside of the allocated range.
class SdfReadOutOfBoundsError : public TfBaseException
{
public:
    using TfBaseException::TfBaseException;
    SDF_API virtual ~SdfReadOutOfBoundsError() override;
};

SdfReadOutOfBoundsError::~SdfReadOutOfBoundsError()
{
}

namespace Sdf_CrateFile
{
// Metafunction that determines if a T instance can be read/written by simple
// bitwise copy.
template <class T>
struct _IsBitwiseReadWrite {
    static const bool value =
        std::is_enum<T>::value ||
        std::is_arithmetic<T>::value ||
        std::is_same<T, GfHalf>::value ||
        std::is_trivial<T>::value ||
        GfIsGfVec<T>::value ||
        GfIsGfMatrix<T>::value ||
        GfIsGfQuat<T>::value ||
        std::is_base_of<Index, T>::value;
};
} // Sdf_CrateFile

namespace {

// We use type char and a deleter for char[] instead of just using
// type char[] due to a (now fixed) bug in libc++ in LLVM.  See
// https://llvm.org/bugs/show_bug.cgi?id=18350.
typedef std::unique_ptr<char, std::default_delete<char[]> > RawDataPtr;

using namespace Sdf_CrateFile;

// To add a new section, add a name here and add that name to _KnownSections
// below, then add handling for it in _Write and _ReadStructuralSections.
constexpr _SectionName _TokensSectionName = "TOKENS";
constexpr _SectionName _StringsSectionName = "STRINGS";
constexpr _SectionName _FieldsSectionName = "FIELDS";
constexpr _SectionName _FieldSetsSectionName = "FIELDSETS";
constexpr _SectionName _PathsSectionName = "PATHS";
constexpr _SectionName _SpecsSectionName = "SPECS";

constexpr _SectionName _KnownSections[] = {
    _TokensSectionName, _StringsSectionName, _FieldsSectionName,
    _FieldSetsSectionName, _PathsSectionName, _SpecsSectionName
};

template <class T>
struct _IsAlwaysInlined : std::integral_constant<
    bool, sizeof(T) <= sizeof(uint32_t) && _IsBitwiseReadWrite<T>::value> {};

template <> struct _IsAlwaysInlined<string> : std::true_type {};
template <> struct _IsAlwaysInlined<TfToken> : std::true_type {};
template <> struct _IsAlwaysInlined<SdfPath> : std::true_type {};
template <> struct _IsAlwaysInlined<SdfAssetPath> : std::true_type {};

template <class T>
struct _TypeEnumFor {};

template <class T>
struct _SupportsArray {};

#define xx(ENUMNAME, _unused1, CPPTYPE, SUPPORTSARRAY)                         \
template <> struct _TypeEnumFor<CPPTYPE> {                                     \
    static const TypeEnum value = TypeEnum::ENUMNAME;                          \
};                                                                             \
template <> struct _SupportsArray<CPPTYPE> {                                   \
    static constexpr bool value = SUPPORTSARRAY;                               \
};
#include "crateDataTypes.h"
#undef xx

template <class T>
static constexpr ValueRep ValueRepFor(uint64_t payload = 0) {
    return ValueRep(_TypeEnumFor<T>::value,
                    _IsAlwaysInlined<T>::value, /*isArray=*/false, payload);
}

template <class T>
static constexpr ValueRep ValueRepForArray(uint64_t payload = 0) {
    return ValueRep(_TypeEnumFor<T>::value,
                    /*isInlined=*/false, /*isArray=*/true, payload);
}

template <class T>
T *RoundToPageAddr(T *addr) {
    return reinterpret_cast<T *>(
        reinterpret_cast<uintptr_t>(addr) & CRATE_PAGEMASK);
}

template <class T>
uint64_t GetPageNumber(T *addr) {
    return reinterpret_cast<uintptr_t>(addr) >> CRATE_PAGESHIFT;
}

// A helper struct for thread_local that uses nullptr initialization as a
// sentinel to prevent guard variable use from being invoked after first
// initialization.
template <class T>
struct _FastThreadLocalBase
{
    static T &Get() {
        static thread_local T *theTPtr = nullptr;
        if (ARCH_LIKELY(theTPtr)) {
            return *theTPtr;
        }
        static thread_local T theT;
        theTPtr = &theT;
        return *theTPtr;
    }
};

// This is a set that's used as a thread-local to guard against assets that
// contain VtValues that recursively claim to contain themselves.  We insert
// ValueReps as we unpack VtValues and if we ever encounter the same rep again,
// we know we've hit a loop and we can error out instead of infinitely
// recursing.
using UnpackRecursionGuard = pxr_tsl::robin_set<ValueRep, TfHash>;
struct _LocalUnpackRecursionGuard
    : _FastThreadLocalBase<UnpackRecursionGuard> {};

} // anon


namespace Sdf_CrateFile {

// XXX: These checks ensure VtValue can hold ValueRep in the lightest
// possible way -- WBN not to rely on internal knowledge of that.
static_assert(std::is_trivially_constructible_v<ValueRep>);
static_assert(std::is_trivially_copyable_v<ValueRep>);
static_assert(std::is_trivially_copy_assignable_v<ValueRep>);
static_assert(std::is_trivially_destructible_v<ValueRep>);

using namespace Sdf_CrateValueInliners;

using std::make_pair;
using std::string;
using std::tuple;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

// Version history:
// 0.12.0: Added support for splines.
// 0.11.0: Added support for relocates in layer metadata.
// 0.10.0: Added support for the pathExpression value type.
//  0.9.0: Added support for the timecode and timecode[] value types.
//  0.8.0: Added support for SdfPayloadListOp values and SdfPayload values with
//         layer offsets.
//  0.7.0: Array sizes written as 64 bit ints.
//  0.6.0: Compressed (scalar) floating point arrays that are either all ints or
//         can be represented efficiently with a lookup table.
//  0.5.0: Compressed (u)int & (u)int64 arrays, arrays no longer store '1' rank.
//  0.4.0: Compressed structural sections.
//  0.3.0: (broken, unused)
//  0.2.0: Added support for prepend and append fields of SdfListOp.
//  0.1.0: Fixed structure layout issue encountered in Windows port.
//         See _PathItemHeader_0_0_1.
//  0.0.1: Initial release.
constexpr uint8_t USDC_MAJOR = 0;
constexpr uint8_t USDC_MINOR = 12;
constexpr uint8_t USDC_PATCH = 0;

CrateFile::Version
CrateFile::Version::FromString(char const *str)
{
    uint32_t maj, min, pat;
    if (sscanf(str, "%u.%u.%u", &maj, &min, &pat) != 3 ||
        maj > 255 || min > 255 || pat > 255) {
        return Version();
    }
    return Version(maj, min, pat);
}

std::string
CrateFile::Version::AsString() const {
    return TfStringPrintf(
        "%" PRId8 ".%" PRId8 ".%" PRId8, majver, minver, patchver);
}

constexpr CrateFile::Version
_SoftwareVersion { USDC_MAJOR, USDC_MINOR, USDC_PATCH };

static CrateFile::Version
_GetVersionForNewlyCreatedFiles() {
    // Read the env setting and try to parse a version.  If that fails to
    // give a version this software is capable of writing, fall back to the
    // _SoftwareVersion.
    string setting = TfGetEnvSetting(USD_WRITE_NEW_USDC_FILES_AS_VERSION);
    auto ver = CrateFile::Version::FromString(setting.c_str());
    if (!ver.IsValid() || !_SoftwareVersion.CanWrite(ver)) {
        TF_WARN("Invalid value '%s' for USD_WRITE_NEW_USDC_FILES_AS_VERSION - "
                "falling back to default '%s'",
                setting.c_str(), DEFAULT_NEW_VERSION);
        ver = CrateFile::Version::FromString(DEFAULT_NEW_VERSION);
    }
    return ver;
}

static CrateFile::Version
GetVersionForNewlyCreatedFiles() {
    static CrateFile::Version ver = _GetVersionForNewlyCreatedFiles();
    return ver;
}

constexpr char const *USDC_IDENT = "PXR-USDC"; // 8 chars.

struct _PathItemHeader_0_0_1 {
    _PathItemHeader_0_0_1() {}
    _PathItemHeader_0_0_1(PathIndex pi, TokenIndex ti, uint8_t bs)
        : index(pi), elementTokenIndex(ti), bits(bs) {}

    // Deriving _BitwiseReadWrite and having members PathIndex and TokenIndex
    // that derive _BitwiseReadWrite caused gcc on linux and mac to leave 4
    // bytes at the head of this structure, making the whole thing 16 bytes,
    // with the members starting at offset 4.  This was revealed in the Windows
    // port since MSVC made this struct 12 bytes, as intended.  To fix this we
    // have two versions of the struct.  Version 0.0.1 files read/write this
    // structure.  Version 0.1.0 and newer read/write the new one.
    uint32_t _unused_padding_;

    PathIndex index;
    TokenIndex elementTokenIndex;
    uint8_t bits;
};
template <>
struct _IsBitwiseReadWrite<_PathItemHeader_0_0_1> : std::true_type {};

struct _PathItemHeader {
    _PathItemHeader() {}
    _PathItemHeader(PathIndex pi, TokenIndex ti, uint8_t bs)
        : index(pi), elementTokenIndex(ti), bits(bs) {}
    static const uint8_t HasChildBit = 1 << 0;
    static const uint8_t HasSiblingBit = 1 << 1;
    static const uint8_t IsPrimPropertyPathBit = 1 << 2;
    PathIndex index;
    TokenIndex elementTokenIndex;
    uint8_t bits;
};
template <>
struct _IsBitwiseReadWrite<_PathItemHeader> : std::true_type {};

struct _ListOpHeader {
    enum _Bits { IsExplicitBit = 1 << 0,
                 HasExplicitItemsBit = 1 << 1,
                 HasAddedItemsBit = 1 << 2,
                 HasDeletedItemsBit = 1 << 3,
                 HasOrderedItemsBit = 1 << 4,
                 HasPrependedItemsBit = 1 << 5,
                 HasAppendedItemsBit = 1 << 6 };

    _ListOpHeader() : bits(0) {}

    template <class T>
    explicit _ListOpHeader(SdfListOp<T> const &op) : bits(0) {
        bits |= op.IsExplicit() ? IsExplicitBit : 0;
        bits |= op.GetExplicitItems().size() ? HasExplicitItemsBit : 0;
        bits |= op.GetAddedItems().size() ? HasAddedItemsBit : 0;
        bits |= op.GetPrependedItems().size() ? HasPrependedItemsBit : 0;
        bits |= op.GetAppendedItems().size() ? HasAppendedItemsBit : 0;
        bits |= op.GetDeletedItems().size() ? HasDeletedItemsBit : 0;
        bits |= op.GetOrderedItems().size() ? HasOrderedItemsBit : 0;
    }

    bool IsExplicit() const { return bits & IsExplicitBit; }

    bool HasExplicitItems() const { return bits & HasExplicitItemsBit; }
    bool HasAddedItems() const { return bits & HasAddedItemsBit; }
    bool HasPrependedItems() const { return bits & HasPrependedItemsBit; }
    bool HasAppendedItems() const { return bits & HasAppendedItemsBit; }
    bool HasDeletedItems() const { return bits & HasDeletedItemsBit; }
    bool HasOrderedItems() const { return bits & HasOrderedItemsBit; }

    uint8_t bits;
};
template <> struct _IsBitwiseReadWrite<_ListOpHeader> : std::true_type {};

CrateFile::_FileRange::~_FileRange()
{
    if (file && hasOwnership) {
        fclose(file);
    }
}

Vt_ArrayForeignDataSource *
CrateFile::_FileMapping::_Impl
::_AddRangeReference(void const *addr, size_t numBytes)
{
    auto iresult = _outstandingRanges.emplace(this, addr, numBytes);
    // If we take the source's count from 0 -> 1, add a reference to the
    // mapping.
    if (iresult.first->NewRef()) {
        TfDelegatedCountIncrement(this);
    }
    return &(*iresult.first);
}

// The 'start' arg must be volatile so we actually emit the "noop" store
// operations that "write" to the pages.
static void
TouchPages(char volatile *start, size_t numPages)
{
    while (numPages--) {
        *start = *start; // Don't change content, but cause a write.  This
                         // forces the VM to detach the page from its mapped
                         // file backing and make it swap-backed instead
                         // (copy-on-write).  This is sometimes called a "silent
                         // store".  No current hw architecture "optimizes out"
                         // silent stores.
        start += CRATE_PAGESIZE;
    }
}

void
CrateFile::_FileMapping::_Impl::_DetachReferencedRanges()
{
    // At this moment, we're guaranteed that no ZeroCopySource objects'
    // reference counts will increase (and in particular go from 0 to 1) since
    // the mapping is being destroyed.  Similarly no new _outstandingRanges
    // can be created.
    for (auto const &zeroCopy: _outstandingRanges) {
        // This is racy, but benign.  If we see a nonzero count that's
        // concurrently being zeroed, we just do possibly unneeded work.  The
        // crucial thing is that we'll never see a zero count that could
        // possibly become nonzero again.
        if (zeroCopy.IsInUse()) {
            // Calculate the page-aligned start address and the number of pages
            // we need to touch.
            auto addrAsInt = reinterpret_cast<uintptr_t>(zeroCopy.GetAddr());
            int64_t pageStart = addrAsInt / CRATE_PAGESIZE;
            int64_t pageEnd =
                ((addrAsInt + zeroCopy.GetNumBytes() - 1) / CRATE_PAGESIZE) + 1;
            // Make the memory range read/copy-on-write.
            char *startAddr =
                reinterpret_cast<char *>(pageStart * CRATE_PAGESIZE);
            if (ArchSetMemoryProtection(
                    startAddr, (pageEnd-pageStart) * CRATE_PAGESIZE,
                    ArchProtectReadWriteCopy)) {
                TouchPages(reinterpret_cast<char *>(pageStart * CRATE_PAGESIZE),
                           pageEnd - pageStart);
            }
            else {
                TF_WARN("could not set address range permissions to "
                        "copy-on-write");
            }
        }
    }
}

CrateFile::_FileMapping::_Impl::ZeroCopySource::ZeroCopySource(
    CrateFile::_FileMapping::_Impl *m,
    void const *addr, size_t numBytes)
    : Vt_ArrayForeignDataSource(_Detached)
    , _mapping(m)
    , _addr(addr)
    , _numBytes(numBytes) {}

bool CrateFile::_FileMapping::_Impl::ZeroCopySource::operator==(
    ZeroCopySource const &other) const {
    return _mapping == other._mapping &&
        _addr == other._addr && _numBytes == other._numBytes;
}

void CrateFile::_FileMapping::_Impl::ZeroCopySource::_Detached(
    Vt_ArrayForeignDataSource *selfBase) {
    auto *self = static_cast<ZeroCopySource *>(selfBase);
    TfDelegatedCountDecrement(self->_mapping);
}

template <class FileMappingPtr>
struct _MmapStream {
    // Mmap streams support zero-copy arrays; direct references into file-mapped
    // memory.
    static constexpr bool SupportsZeroCopy = true;
    
    explicit _MmapStream(FileMappingPtr const &mapping, char *debugPageMap)
        : _cur(mapping->GetMapStart())
        , _mapping(mapping)
        , _debugPageMap(debugPageMap)
        , _prefetchKB(_GetMMapPrefetchKB()) {}

    _MmapStream &DisablePrefetch() {
        _prefetchKB = 0;
        return *this;
    }
    
    inline void Read(void *dest, size_t nBytes) {
        // Range check first.
        if constexpr (SafetyOverSpeed) {
            char const *mapStart = _mapping->GetMapStart();
            size_t mapLen = _mapping->GetLength();
            
            bool inRange = mapStart <= _cur &&
                (_cur + nBytes) <= (mapStart + mapLen);
            
            if (ARCH_UNLIKELY(!inRange)) {
                ptrdiff_t offset = _cur - mapStart;
                PXR_TF_THROW(SdfReadOutOfBoundsError, TfStringPrintf(
                    "Read out-of-bounds: %zd bytes at offset %td in "
                    "a mapping of length %zd",
                    nBytes, offset, mapLen));
            }
        }

        if (ARCH_UNLIKELY(_debugPageMap)) {
            auto mapStart = _mapping->GetMapStart();
            int64_t pageZero = GetPageNumber(mapStart);
            int64_t firstPage = GetPageNumber(_cur) - pageZero;
            int64_t lastPage = GetPageNumber(_cur + nBytes - 1) - pageZero;
            memset(_debugPageMap + firstPage, 1, lastPage - firstPage + 1);
        }

        if (_prefetchKB) {
            // Custom aligned chunk "prefetch".
            auto mapStart = _mapping->GetMapStart();
            auto mapStartPage = RoundToPageAddr(mapStart);
            const auto chunkBytes = _prefetchKB * 1024;
            auto firstChunk = (_cur-mapStartPage) / chunkBytes;
            auto lastChunk = ((_cur-mapStartPage) + nBytes) / chunkBytes;
            
            char const *beginAddr = mapStartPage + firstChunk * chunkBytes;
            char const *endAddr = mapStartPage + std::min(
                _mapping->GetLength() + (mapStart-mapStartPage),
                (lastChunk + 1) * chunkBytes);
            
            ArchMemAdvise(reinterpret_cast<void *>(
                              const_cast<char *>(beginAddr)),
                          endAddr-beginAddr, ArchMemAdviceWillNeed);
        }

        memcpy(dest, _cur, nBytes);
        
        _cur += nBytes;
    }
    inline int64_t Tell() const {
        return _cur - _mapping->GetMapStart();
    }
    inline void Seek(int64_t offset) {
        _cur = _mapping->GetMapStart() + offset;
    }
    inline void Prefetch(int64_t offset, int64_t size) {
        ArchMemAdvise(
            _mapping->GetMapStart() + offset, size, ArchMemAdviceWillNeed);
    }

    Vt_ArrayForeignDataSource *
    CreateZeroCopyDataSource(void const *addr, size_t numBytes) {
        char const *mapStart = _mapping->GetMapStart();
        char const *chAddr = static_cast<char const *>(addr);
        size_t mapLen = _mapping->GetLength();
        bool inRange = mapStart <= chAddr &&
            (chAddr + numBytes) <= (mapStart + mapLen);
        
        if (ARCH_UNLIKELY(!inRange)) {
            ptrdiff_t offset = chAddr - mapStart;
            TF_RUNTIME_ERROR(
                "Zero-copy data range out-of-bounds: %zd bytes at offset "
                "%td in a mapping of length %zd",
                numBytes, offset, mapLen);
            return nullptr;
        }
        return _mapping->AddRangeReference(addr, numBytes);
    }

    inline void const *TellMemoryAddress() const {
        return _cur;
    }
    
private:
    char const *_cur;
    FileMappingPtr _mapping;
    char *_debugPageMap;
    int _prefetchKB;
};

template <class FileMappingPtr>
inline _MmapStream<FileMappingPtr>
_MakeMmapStream(FileMappingPtr const &mapping, char *debugPageMap) {
    return _MmapStream<FileMappingPtr>(mapping, debugPageMap);
}

struct _PreadStream {
    // Pread streams do not support zero-copy arrays.
    static constexpr bool SupportsZeroCopy = false;

    template <class FileRange>
    explicit _PreadStream(FileRange const &fr)
        : _start(fr.startOffset)
        , _cur(0)
        , _file(fr.file) {}
    inline void Read(void *dest, size_t nBytes) {
        int64_t nRead = ArchPRead(_file, dest, nBytes, _start + _cur);
        if constexpr (SafetyOverSpeed) {
            if (ARCH_UNLIKELY(nRead != static_cast<int64_t>(nBytes))) {
                PXR_TF_THROW(SdfReadOutOfBoundsError, TfStringPrintf(
                             "Failed reading %zu bytes at offset %" PRId64,
                             nBytes, _start + _cur));
            }
        }
        _cur += nRead;
    }
    inline int64_t Tell() const { return _cur; }
    inline void Seek(int64_t offset) { _cur = offset; }
    inline void Prefetch(int64_t offset, int64_t size) {
        ArchFileAdvise(_file, _start+offset, size, ArchFileAdviceWillNeed);
    }

private:
    int64_t _start;
    int64_t _cur;
    FILE *_file;
};

struct _AssetStream {
    // Asset streams do not support zero-copy arrays.
    static constexpr bool SupportsZeroCopy = false;

    explicit _AssetStream(ArAssetSharedPtr const &asset)
        : _asset(asset)
        , _cur(0) {}
    inline void Read(void *dest, size_t nBytes) {
        size_t nRead = _asset->Read(dest, nBytes, _cur);
        if constexpr (SafetyOverSpeed) {
            if (nRead != nBytes) {
                PXR_TF_THROW(SdfReadOutOfBoundsError, TfStringPrintf(
                             "Failed reading %zu bytes at offset %zu",
                             nBytes, _cur));
            }
        }
        _cur += nRead;
    }
    inline int64_t Tell() const { return _cur; }
    inline void Seek(int64_t offset) { _cur = offset; }
    inline void Prefetch(int64_t offset, int64_t size) {
        /* no prefetch impl */
    }

private:
    ArAssetSharedPtr _asset;
    int64_t _cur;
};

////////////////////////////////////////////////////////////////////////
// _TableOfContents
CrateFile::_Section const *
CrateFile::_TableOfContents::GetSection(_SectionName name) const
{
    for (auto const &sec: sections) {
        if (name == sec.name)
            return &sec;
    }
    TF_RUNTIME_ERROR("Crate file missing %s section", name.c_str());
    return nullptr;
}

int64_t
CrateFile::_TableOfContents::GetMinimumSectionStart() const
{
    auto theMin = std::min_element(
        sections.begin(), sections.end(),
        [](_Section const &l, _Section const &r) { return l.start < r.start; });

    return theMin == sections.end() ? sizeof(_BootStrap) : theMin->start;
}

////////////////////////////////////////////////////////////////////////
// _BufferedOutput
class CrateFile::_BufferedOutput
{
public:
    using OutputType = ArWritableAsset*;

    // Current buffer size is 512k.
    static const size_t BufferCap = 512*1024;

    // Helper move-only buffer object -- memory + valid size.
    struct _Buffer {
        _Buffer() = default;
        _Buffer(_Buffer const &) = delete;
        _Buffer &operator=(_Buffer const &) = delete;
        _Buffer(_Buffer &&) = default;
        _Buffer &operator=(_Buffer &&) = default;

        RawDataPtr bytes { new char[BufferCap] };
        int64_t size = 0;
    };

    explicit _BufferedOutput(OutputType file)
        : _filePos(0)
        , _file(file)
        , _bufferPos(0)
        , _writeTask(_dispatcher, [this]() { _DoWrites(); }) {
        // Create NumBuffers buffers.  One is _buffer, the remainder live in
        // _freeBuffers.
        constexpr const int NumBuffers = 8;
        for (int i = 1; i != NumBuffers; ++i) {
            _freeBuffers.push(_Buffer());
        }
    }

    inline void Flush() {
        _FlushBuffer();
        _dispatcher.Wait();
    }

    inline void Write(void const *bytes, int64_t nBytes) {
        // Write and flush as needed.
        while (nBytes) {
            int64_t available = BufferCap - (_filePos - _bufferPos);
            int64_t numToWrite = std::min(available, nBytes);
            
            _WriteToBuffer(bytes, numToWrite);
            
            bytes = static_cast<char const *>(bytes) + numToWrite;
            nBytes -= numToWrite;

            if (numToWrite == available)
                _FlushBuffer();
        }
    }

    inline int64_t Tell() const { return _filePos; }

    inline void Seek(int64_t offset) {
        // If the seek lands in a valid buffer region, then just adjust the
        // _filePos.  Otherwise _FlushBuffer() and reset.
        if (offset >= _bufferPos && offset <= (_bufferPos + _buffer.size)) {
            _filePos = offset;
        }
        else {
            _FlushBuffer();
            _bufferPos = _filePos = offset;
        }
    }

    // Seek to the next position that's a multiple of \p alignment.  Alignment
    // must be a power-of-two.
    inline int64_t Align(int alignment) {
        Seek((Tell() + alignment - 1) & ~(alignment - 1));
        return Tell();
    }        

private:
    inline void _FlushBuffer() {
        if (_buffer.size) {
            // Queue a write of _buffer bytes to the file at _bufferPos.  Set
            // _bufferPos to be _filePos.
            _QueueWrite(std::move(_buffer), _bufferPos);
            // Get a new _buffer.  May have to wait if all are pending writes.
            while (!_freeBuffers.try_pop(_buffer))
                _dispatcher.Wait();
        }
        // Adjust the buffer to start at the write head.
        _bufferPos = _filePos;
    }

    inline void _WriteToBuffer(void const *bytes, int64_t nBytes) {
        // Fill the buffer, update its size and update the write head. Caller
        // guarantees no overrun.
        int64_t writeStart = (_filePos - _bufferPos);
        if (writeStart + nBytes > _buffer.size) {
            _buffer.size = writeStart + nBytes;
        }
        void *bufPtr = static_cast<void *>(_buffer.bytes.get() + writeStart);
        memcpy(bufPtr, bytes, nBytes);
        _filePos += nBytes;
    }
    
    // Move-only write operation for the writer task to process.
    struct _WriteOp {
        _WriteOp() = default;
        _WriteOp(_WriteOp const &) = delete;
        _WriteOp(_WriteOp &&) = default;
        _WriteOp &operator=(_WriteOp &&) = default;
        _WriteOp(_Buffer &&buf, int64_t pos) : buf(std::move(buf)), pos(pos) {}
        _Buffer buf;
        int64_t pos = 0;
    };

    inline int64_t _QueueWrite(_Buffer &&buf, int64_t pos) {
        // Arrange to write the buffered data.  Enqueue the op and wake the
        // writer task.
        int64_t sz = static_cast<int64_t>(buf.size);
        _writeQueue.push(_WriteOp(std::move(buf), pos));
        _writeTask.Wake();
        return sz;
    }

    void _DoWrites() {
        // This is the writer task.  It just pops off ops and writes them, then
        // moves the buffer to the free list.
        _WriteOp op;
        while (_writeQueue.try_pop(op)) {
            // Write the bytes.
            WriteToAsset(_file, op.buf.bytes.get(), op.buf.size, op.pos);
            // Add the buffer back to _freeBuffers for reuse.
            op.buf.size = 0;
            _freeBuffers.push(std::move(op.buf));
        }
    }
    
    // Write head in the file.  Always inside the buffer region.
    int64_t _filePos;
    OutputType _file;

    // Start of current buffer is at this file offset.
    int64_t _bufferPos;
    _Buffer _buffer;

    // Queue of free buffer objects.
    tbb::concurrent_queue<_Buffer> _freeBuffers;
    // Queue of pending write operations.
    tbb::concurrent_queue<_WriteOp> _writeQueue;

    WorkDispatcher _dispatcher;
    WorkSingularTask _writeTask;
};

////////////////////////////////////////////////////////////////////////
// _PackingContext
struct CrateFile::_PackingContext
{
    using OutputType = ArWritableAssetSharedPtr;
    static ArWritableAsset* _Get(OutputType& out) { return out.get(); }

    _PackingContext() = delete;
    _PackingContext(_PackingContext const &) = delete;
    _PackingContext &operator=(_PackingContext const &) = delete;

    _PackingContext(CrateFile *crate, 
                    OutputType &&outAsset,
                    std::string const &fileName)
        : fileName(fileName)
        , writeVersion(crate->_assetPath.empty() ?
                       GetVersionForNewlyCreatedFiles() :
                       Version(crate->_boot))
        , bufferedOutput(_Get(outAsset))
        , outputAsset(std::move(outAsset)) {
        
        // Populate this context with everything we need from \p crate in order
        // to do deduplication, etc.
        WorkDispatcher wd;
        
        // Read in any unknown sections so we can rewrite them later.
        wd.Run([this, crate]() {
            for (auto const &sec: crate->_toc.sections) {
                if (!_IsKnownSection(sec.name)) {
                    unknownSections.emplace_back(
                        sec.name, _ReadSectionBytes(sec, crate),
                        sec.size);
                }
            }
        });
        
        // Ensure that pathToPathIndex is correctly populated.
        wd.Run([this, crate]() {
            for (size_t i = 0; i != crate->_paths.size(); ++i)
                pathToPathIndex[crate->_paths[i]] = PathIndex(i);
        });
        
        // Ensure that fieldToFieldIndex is correctly populated.
        wd.Run([this, crate]() {
            for (size_t i = 0; i != crate->_fields.size(); ++i)
                fieldToFieldIndex[
                    crate->_fields[i]] = FieldIndex(i);
        });
        
        // Ensure that fieldsToFieldSetIndex is correctly populated.
        auto const &fsets = crate->_fieldSets;
        wd.Run([this, &fsets]() {
            vector<FieldIndex> fieldIndexes;
            for (auto fsBegin = fsets.begin(),
                     fsEnd = find(
                         fsBegin, fsets.end(), FieldIndex());
                 fsBegin != fsets.end();
                 fsBegin = fsEnd + 1,
                     fsEnd = find(
                         fsBegin, fsets.end(), FieldIndex())) {
                fieldIndexes.assign(fsBegin, fsEnd);
                fieldsToFieldSetIndex[fieldIndexes] =
                    FieldSetIndex(fsBegin - fsets.begin());
            }
        });
        
        // Ensure that tokenToTokenIndex is correctly populated.
        wd.Run([this, crate]() {
            for (size_t i = 0; i != crate->_tokens.size(); ++i)
                tokenToTokenIndex[
                    crate->_tokens[i]] = TokenIndex(i);
        });
        
        // Ensure that stringToStringIndex is correctly populated.
        wd.Run([this, crate]() {
            for (size_t i = 0; i != crate->_strings.size(); ++i)
                stringToStringIndex[
                    crate->GetString(StringIndex(i))] =
                    StringIndex(i);
        });
        
        // Set file pos to start of the structural sections in the current TOC.
        bufferedOutput.Seek(crate->_toc.GetMinimumSectionStart());
    }

    // Close output asset.  No further writes may be done.
    bool CloseOutputAsset() {
        return outputAsset->Close();
    }
   
    // Inform the writer that the output stream requires the given version
    // (or newer) to be read back.  This allows the writer to start with
    // a conservative version assumption and promote to newer versions
    // only as required by the data stream contents.
    bool RequestWriteVersionUpgrade(Version ver, std::string reason) {
        if (!writeVersion.CanRead(ver)) {
            TF_WARN("Upgrading crate file <%s> from version %s to %s: %s",
                    fileName.c_str(),
                    writeVersion.AsString().c_str(), ver.AsString().c_str(),
                    reason.c_str());
            writeVersion = ver;
        }
        // For now, this always returns true, indicating success.  In
        // the future, we anticipate a mechanism to confirm the upgrade
        // is desired -- in which case this could return true or false.
        return true;
    }

    // Read the bytes of some unknown section into memory so we can rewrite them
    // out later (to preserve it).
    RawDataPtr
    _ReadSectionBytes(_Section const &sec, CrateFile *crate) const {
        RawDataPtr result(new char[sec.size]);
        crate->_ReadRawBytes(sec.start, sec.size, result.get());
        return result;
    }

    // Deduplication tables.
    unordered_map<TfToken, TokenIndex, _Hasher> tokenToTokenIndex;
    unordered_map<string, StringIndex, _Hasher> stringToStringIndex;
    unordered_map<SdfPath, PathIndex, SdfPath::Hash> pathToPathIndex;
    unordered_map<Field, FieldIndex, _Hasher> fieldToFieldIndex;
    
    // A mapping from a group of fields to their starting index in _fieldSets.
    unordered_map<vector<FieldIndex>,
                  FieldSetIndex, _Hasher> fieldsToFieldSetIndex;
    
    // Unknown sections we're moving to the new structural area.
    vector<tuple<string, RawDataPtr, size_t>> unknownSections;

    // Filename we're writing to.
    std::string fileName;
    // Version we're writing.
    Version writeVersion;
    // BufferedOutput helper.
    _BufferedOutput bufferedOutput;
    // Output destination.
    OutputType outputAsset;
};

/////////////////////////////////////////////////////////////////////////
// Reader

template <class ByteStream>
class CrateFile::_Reader
{
    void _RecursiveRead() {
        auto start = src.Tell();
        auto offset = Read<int64_t>();
        src.Seek(start + offset);
    }

    void _RecursiveReadAndPrefetch() {
        auto start = src.Tell();
        auto offset = Read<int64_t>();
        src.Prefetch(start, offset);
        src.Seek(start + offset);
    }

    // Read implementations that need partial specialization.
    template <class T>
    SdfListOp<T> _Read(SdfListOp<T> *) {
        SdfListOp<T> listOp;
        auto h = Read<_ListOpHeader>();
        if (h.IsExplicit()) { listOp.ClearAndMakeExplicit(); }
        if (h.HasExplicitItems()) {
            listOp.SetExplicitItems(Read<vector<T>>()); }
        if (h.HasAddedItems()) { listOp.SetAddedItems(Read<vector<T>>()); }
        if (h.HasPrependedItems()) {
            listOp.SetPrependedItems(Read<vector<T>>()); }
        if (h.HasAppendedItems()) {
            listOp.SetAppendedItems(Read<vector<T>>()); }
        if (h.HasDeletedItems()) { listOp.SetDeletedItems(Read<vector<T>>()); }
        if (h.HasOrderedItems()) { listOp.SetOrderedItems(Read<vector<T>>()); }
        return listOp;
    }
    
    template <class T>
    vector<T> _Read(vector<T> *) {
        auto sz = Read<uint64_t>();
        vector<T> vec(sz);
        ReadContiguous(vec.data(), sz);
        return vec;
    }

public:
    static constexpr bool StreamSupportsZeroCopy = ByteStream::SupportsZeroCopy;
    
    _Reader(CrateFile const *crate, ByteStream &src)
        : crate(crate)
        , src(src) {}

    void Prefetch(int64_t offset, int64_t size) { src.Prefetch(offset, size); }

    void Seek(uint64_t offset) { src.Seek(offset); }

    template <class T>
    T GetUninlinedValue(uint32_t x) const {
        if constexpr (std::is_same_v<T, string>) {
            return crate->GetString(StringIndex(x));
        }
        else if constexpr (std::is_same_v<T, TfToken>) {
            return crate->GetToken(TokenIndex(x));
        }
        else if constexpr (std::is_same_v<T, SdfPath>) {
            return crate->GetPath(PathIndex(x));
        }
        else if constexpr (std::is_same_v<T, SdfAssetPath>) {
            return SdfAssetPath(crate->GetToken(TokenIndex(x)));
        }
        else if constexpr (std::is_same_v<T, SdfVariability>) {
            // Explicitly convert legacy SdfVariabilityConfig value to
            // SdfVariabilityUniform. This "config" variability was never used
            // in USD but clients may have written this value out so we need to
            // handle it to maintain backwards compatibility.
            static const uint32_t LEGACY_CONFIG_VARIABILITY = 2;
            if (x == LEGACY_CONFIG_VARIABILITY) {
                return SdfVariabilityUniform;
            }
            return static_cast<SdfVariability>(x);
        }
        else {
            static_assert(sizeof(T) <= sizeof(x), "");
            T r;
            char const *srcBytes = reinterpret_cast<char const *>(&x);
            char *dstBytes = reinterpret_cast<char *>(&r);
            memcpy(dstBytes, srcBytes, sizeof(r));
            return r;
        }
    }

    // Map helper.
    template <class Map>
    Map ReadMap() {
        Map map;
        auto sz = Read<uint64_t>();
        while (sz--) {
            // Do not combine the following into one statement.  It must be
            // separate because the two modifications to 'src' must be correctly
            // sequenced.
            auto key = Read<typename Map::key_type>();
            map[key] = Read<typename Map::mapped_type>();
        }
        return map;
    }

    ////////////////////////////////////////////////////////////////////////
    // Main entry point Read() to read an object from the stream.
    template <class T>
    inline T Read() {
        if constexpr (_IsBitwiseReadWrite<T>::value) {
            T bits;
            src.Read(&bits, sizeof(bits));
            return bits;
        }
        else if constexpr (std::is_same_v<T, _TableOfContents>) {
            _TableOfContents ret;
            ret.sections = Read<decltype(ret.sections)>();
            return ret;
        }
        else if constexpr (std::is_same_v<T, string>) {
            return crate->GetString(Read<StringIndex>());
        }
        else if constexpr (std::is_same_v<T, TfToken>) {
            return crate->GetToken(Read<TokenIndex>());
        }
        else if constexpr (std::is_same_v<T, SdfPath>) {
            return crate->GetPath(Read<PathIndex>());
        }
        else if constexpr (std::is_same_v<T, SdfRelocate>) {
            // Do not combine the following into one statement.  They must be
            // separate because the two modifications to the 'src' member must
            // be correctly sequenced.
            auto sourcePath = Read<SdfPath>();
            auto targetPath = Read<SdfPath>();
            return SdfRelocate(std::move(sourcePath), std::move(targetPath));
        }
        else if constexpr (std::is_same_v<T, VtDictionary> ||
                           std::is_same_v<T, SdfVariantSelectionMap>) {
            return ReadMap<T>();
        }
        else if constexpr (std::is_same_v<T, SdfAssetPath> ||
                           std::is_same_v<T, SdfPathExpression>) {
            return T(Read<string>());
        }
        else if constexpr (std::is_same_v<T, SdfTimeCode>) {
            return SdfTimeCode(Read<double>());
        }
        else if constexpr (std::is_same_v<T, SdfUnregisteredValue>) {
            VtValue val = Read<VtValue>();
            if (val.IsHolding<string>())
                return SdfUnregisteredValue(val.UncheckedGet<string>());
            if (val.IsHolding<VtDictionary>())
                return SdfUnregisteredValue(val.UncheckedGet<VtDictionary>());
            if (val.IsHolding<SdfUnregisteredValueListOp>())
                return SdfUnregisteredValue(
                    val.UncheckedGet<SdfUnregisteredValueListOp>());
            TF_CODING_ERROR("SdfUnregisteredValue in crate file contains "
                            "invalid type '%s' = '%s'; expected string, "
                            "VtDictionary or SdfUnregisteredValueListOp; "
                            "returning empty", val.GetTypeName().c_str(),
                            TfStringify(val).c_str());
            return SdfUnregisteredValue();
        }
        else if constexpr (std::is_same_v<T, SdfLayerOffset>) {
            // Do not combine the following into one statement.  They must be
            // separate because the two modifications to the 'src' member must
            // be correctly sequenced.
            auto offset = Read<double>();
            auto scale = Read<double>();
            return SdfLayerOffset(offset, scale);
        }
        else if constexpr (std::is_same_v<T, SdfReference>) {
            // Do not combine the following into one statement.  They must be
            // separate because the two modifications to the 'src' member must
            // be correctly sequenced.
            auto assetPath = Read<string>();
            auto primPath = Read<SdfPath>();
            auto layerOffset = Read<SdfLayerOffset>();
            auto customData = Read<VtDictionary>();
            return SdfReference(std::move(assetPath), std::move(primPath),
                                std::move(layerOffset), std::move(customData));
        }
        else if constexpr (std::is_same_v<T, SdfPayload>) {
            // Do not combine the following into one statement.  They must be
            // separate because the two modifications to the 'src' member must
            // be correctly sequenced.
            auto assetPath = Read<string>();
            auto primPath = Read<SdfPath>();
            
            // Layer offsets were added to SdfPayload starting in 0.8.0. Files 
            // before that cannot have them.
            const bool canReadLayerOffset = 
                (Version(crate->_boot) >= Version(0, 8, 0));
            if (canReadLayerOffset) {
                auto layerOffset = Read<SdfLayerOffset>();
                return SdfPayload(assetPath, primPath, layerOffset);
            } else {
                return SdfPayload(assetPath, primPath);
            }
        }
        else if constexpr (std::is_same_v<T, VtValue>) {
            _RecursiveReadAndPrefetch();
            auto rep = Read<ValueRep>();
            // Guard against recursion here -- a bad file can cause infinite
            // recursion via VtValues that claim to contain themselves.
            auto &recursionGuard = _LocalUnpackRecursionGuard::Get();
            VtValue result;
            if (!recursionGuard.insert(rep).second) {
                TF_RUNTIME_ERROR("Corrupt asset <%s>: a VtValue claims to "
                                 "recursively contain itself -- returning "
                                 "an empty VtValue instead",
                                 crate->GetAssetPath().c_str());
            }
            else {
                result = crate->UnpackValue(rep);
            }
            recursionGuard.erase(rep);
            return result;
        }
        else if constexpr (std::is_same_v<T, TimeSamples>) {
            TimeSamples ret;
            
            // Reconstitute a rep for this very location in the file to be
            // retained in the TimeSamples result.
            ret.valueRep = ValueRepFor<TimeSamples>(src.Tell());
            
            _RecursiveRead();
            auto timesRep = Read<ValueRep>();
            
            // Deduplicate times in-memory by ValueRep.  Optimistically take the
            // read lock and see if we already have times.
            tbb::spin_rw_mutex::scoped_lock
                lock(crate->_sharedTimesMutex, /*write=*/false);
            auto sharedTimesIter = crate->_sharedTimes.find(timesRep);
            if (sharedTimesIter != crate->_sharedTimes.end()) {
                // Yes, reuse existing times.
                ret.times = sharedTimesIter->second;
            } else {
                // The lock upgrade here may or may not be atomic.  This means
                // someone else may have populated the table while we were
                // upgrading.
                lock.upgrade_to_writer();
                auto iresult =
                    crate->_sharedTimes.emplace(timesRep, Sdf_EmptySharedTag);
                if (iresult.second) {
                    // We get to do the population.
                    auto sharedTimes = TimeSamples::SharedTimes();
                    crate->_UnpackValue(timesRep, &sharedTimes.GetMutable());
                    iresult.first->second.swap(sharedTimes);
                }
                ret.times = iresult.first->second;
            }
            lock.release();
            
            _RecursiveRead();
            
            // Store the offset to the value reps in the file.  The values are
            // encoded as a uint64_t size followed by contiguous reps.  So we
            // jump over that uint64_t and store the start of the reps.  Then we
            // seek forward past the reps to continue.
            auto numValues = Read<uint64_t>();
            ret.valuesFileOffset = src.Tell();
            
            // Now move past the reps to continue.
            src.Seek(ret.valuesFileOffset + numValues * sizeof(ValueRep));

            return ret;
        }
        else if constexpr (std::is_same_v<T, TsSpline>) {
            // Splines are a data blob plus a customData map.
            vector<uint8_t> splineData = Read<vector<uint8_t>>();
            std::unordered_map<double, VtDictionary> customData =
                ReadMap<std::unordered_map<double, VtDictionary>>();
            return Ts_BinaryDataAccess::CreateSplineFromBinaryData(
                splineData, std::move(customData));
        }
        else {
            // Otherwise read partial-specialized stuff.
            return this->_Read(static_cast<T *>(nullptr));
        }
    }

    template <class T>
    void ReadContiguous(T *values, size_t sz) {
        if constexpr (_IsBitwiseReadWrite<T>::value) {
            src.Read(static_cast<void *>(values), sz * sizeof(*values));
        }
        else {
            std::for_each(values, values + sz, [this](T &v) { v = Read<T>(); });
        }
    }
    
    CrateFile const *crate;
    ByteStream src;
};

template <class ByteStream>
CrateFile::_Reader<ByteStream>
CrateFile::_MakeReader(ByteStream src) const
{
    return _Reader<ByteStream>(this, src);
}

/////////////////////////////////////////////////////////////////////////
// Writers
class CrateFile::_Writer
{
public:
    explicit _Writer(CrateFile *crate)
        : crate(crate)
        , sink(&crate->_packCtx->bufferedOutput) {}

    // Recursive write helper.  We use these when writing values if we may
    // invoke _PackValue() recursively.  Since _PackValue() may or may not write
    // to the file, we need to account for jumping over that written nested
    // data, and this function automates that.
    template <class Fn>
    void _RecursiveWrite(Fn const &fn) {
        // Reserve space for a forward offset to where the primary data will
        // live.
        int64_t offsetLoc = Tell();
        WriteAs<int64_t>(0);
        // Invoke the writing function, which may write arbitrary data.
        fn();
        // Now that we know where the primary data will end up, seek back and
        // write the offset value, then seek forward again.
        int64_t end = Tell();
        Seek(offsetLoc);
        WriteAs<int64_t>(end - offsetLoc);
        Seek(end);
    }

public:

    int64_t Tell() const { return sink->Tell(); }
    void Seek(int64_t offset) { sink->Seek(offset); }
    void Flush() { sink->Flush(); }
    int64_t Align(int alignment) { return sink->Align(alignment); }

    template <class T>
    uint32_t GetInlinedValue(T x) {
        uint32_t r = 0;
        static_assert(sizeof(x) <= sizeof(r), "");
        memcpy(&r, &x, sizeof(x));
        return r;
    }

    uint32_t GetInlinedValue(string const &s) {
        return crate->_AddString(s).value;
    }

    uint32_t GetInlinedValue(TfToken const &t) {
        return crate->_AddToken(t).value;
    }

    uint32_t GetInlinedValue(SdfPath const &p) {
        return crate->_AddPath(p).value;
    }

    uint32_t GetInlinedValue(SdfAssetPath const &p) {
        return crate->_AddToken(TfToken(p.GetAssetPath())).value;
    }

    ////////////////////////////////////////////////////////////////////////
    // Basic Write
    template <class T>
    typename std::enable_if<_IsBitwiseReadWrite<T>::value>::type
    Write(T const &bits) { sink->Write(&bits, sizeof(bits)); }

    template <class U, class T>
    void WriteAs(T const &obj) { return Write(static_cast<U>(obj)); }

    // Map helper.
    template <class Map>
    void WriteMap(Map const &map) {
        WriteAs<uint64_t>(map.size());
        for (auto const &kv: map) {
            Write(kv.first);
            Write(kv.second);
        }
    }

    void Write(_TableOfContents const &toc) { Write(toc.sections); }
    void Write(std::string const &str) { Write(crate->_AddString(str)); }
    void Write(TfToken const &tok) { Write(crate->_AddToken(tok)); }
    void Write(SdfPath const &path) { Write(crate->_AddPath(path)); }
    void Write(VtDictionary const &dict) { WriteMap(dict); }
    void Write(SdfAssetPath const &ap) { Write(ap.GetAssetPath()); }
    void Write(SdfTimeCode const &tc) { 
        crate->_packCtx->RequestWriteVersionUpgrade(
            Version(0, 9, 0),
            "A timecode or timecode[] value type was detected which requires "
            "crate version 0.9.0.");
        Write(tc.GetValue()); 
    }
    void Write(SdfPathExpression const &pathExpr) {
        crate->_packCtx->RequestWriteVersionUpgrade(
            Version(0,10,0),
            "A pathExpression value type was detected which requires crate "
            "version 0.10.0.");
        Write(pathExpr.GetText());
    }
    void Write(SdfUnregisteredValue const &urv) { Write(urv.GetValue()); }
    void Write(SdfVariantSelectionMap const &vsmap) { WriteMap(vsmap); }
    void Write(SdfLayerOffset const &layerOffset) {
        Write(layerOffset.GetOffset());
        Write(layerOffset.GetScale());
    }
    void Write(SdfReference const &ref) {
        Write(ref.GetAssetPath());
        Write(ref.GetPrimPath());
        Write(ref.GetLayerOffset());
        Write(ref.GetCustomData());
    }
    void Write(SdfPayload const &ref) {
        // Layer offsets in payloads are only supported in version 0.8 and 
        // later.  If we have to write one, we may have to upgrade the version
        if (!ref.GetLayerOffset().IsIdentity()) {
            crate->_packCtx->RequestWriteVersionUpgrade(
                Version(0, 8, 0),
                "A payload with a non-identity layer offset "
                "was detected, which requires crate version 0.8.0.");
        }
        Write(ref.GetAssetPath());
        Write(ref.GetPrimPath());

        // Always write layer offsets in files versioned 0.8.0 or later
        if (crate->_packCtx->writeVersion >= Version(0, 8, 0)) {
            Write(ref.GetLayerOffset());
        } 
    }
    template <class T>
    void Write(SdfListOp<T> const &listOp) {
        _ListOpHeader h(listOp);
        if (h.HasPrependedItems() || h.HasAppendedItems()) {
            crate->_packCtx->RequestWriteVersionUpgrade(
                Version(0, 2, 0),
                "A SdfListOp value using a prepended or appended value "
                "was detected, which requires crate version 0.2.0.");
        }
        Write(h);
        if (h.HasExplicitItems()) { Write(listOp.GetExplicitItems()); }
        if (h.HasAddedItems()) { Write(listOp.GetAddedItems()); }
        if (h.HasPrependedItems()) { Write(listOp.GetPrependedItems()); }
        if (h.HasAppendedItems()) { Write(listOp.GetAppendedItems()); }
        if (h.HasDeletedItems()) { Write(listOp.GetDeletedItems()); }
        if (h.HasOrderedItems()) { Write(listOp.GetOrderedItems()); }
    }
    // Specialized override for payload list ops which require a version check.
    void Write(SdfPayloadListOp const &listOp) {
        crate->_packCtx->RequestWriteVersionUpgrade(
            Version(0, 8, 0),
            "A SdfPayloadListOp value was detected which requires crate "
            "version 0.8.0.");
        Write<SdfPayload>(listOp);
    }
    void Write(SdfRelocate const &relocate) {
        crate->_packCtx->RequestWriteVersionUpgrade(
            Version(0, 11, 0),
            "A SdfRelocatesMap value was detected which requires crate "
            "version 0.11.0.");
         Write(relocate.first);
         Write(relocate.second);
    }
    void Write(VtValue const &val) {
        ValueRep rep;
        _RecursiveWrite(
            [this, &val, &rep]() { rep = crate->_PackValue(val); });
        Write(rep);
    }

    void Write(TimeSamples const &samples) {
        // Pack the times to deduplicate.
        ValueRep timesRep;
        _RecursiveWrite([this, &timesRep, &samples]() {
                timesRep = crate->_PackValue(samples.times.Get());
            });
        Write(timesRep);

        // Pack the individual elements, to deduplicate them.
        vector<ValueRep> reps(samples.values.size());
        _RecursiveWrite([this, &reps, &samples]() {
                transform(samples.values.begin(), samples.values.end(),
                          reps.begin(),
                          [this](VtValue const &val) {
                              return crate->_PackValue(val);
                          });
            });

        // Write size and contiguous reps.
        WriteAs<uint64_t>(reps.size());
        WriteContiguous(reps.data(), reps.size());
    }

    void Write(const TsSpline &spline) {
        // Make sure our output format is compatible with splines.  If the
        // spline binary format is updated, we must rev the required version
        // here as well; the static_assert is here to remind us.
        static_assert(Ts_BinaryDataAccess::GetBinaryFormatVersion() == 1);
        crate->_packCtx->RequestWriteVersionUpgrade(
            Version(0,12,0),
            "A spline was detected which requires crate "
            "version 0.12.0.");

        // Splines are a data blob plus a customData map.
        vector<uint8_t> splineData;
        const std::unordered_map<double, VtDictionary> *customData = nullptr;
        Ts_BinaryDataAccess::GetBinaryData(spline, &splineData, &customData);
        Write(splineData);
        WriteMap(*customData);
    }

    template <class T>
    void Write(vector<T> const &vec) {
        WriteAs<uint64_t>(vec.size());
        WriteContiguous(vec.data(), vec.size());
    }

    template <class T>
    void WriteContiguous(T const *values, size_t sz) {
        if constexpr (_IsBitwiseReadWrite<T>::value) {
            sink->Write(values, sizeof(*values) * sz);
        }
        else {
            std::for_each(values, values + sz,
                          [this](T const &v) { Write(v); });
        }            
    }

    CrateFile *crate;
    _BufferedOutput *sink;
};


////////////////////////////////////////////////////////////////////////
// ValueHandler class template -- supports top-level value pack/unpack.

template <class T>
struct CrateFile::_ValueHandler : _ValueHandlerBase
{
    inline ValueRep Pack(_Writer writer, T const &val) {
        if constexpr (_IsAlwaysInlined<T>::value) {
            // Inline it into the rep.
            return ValueRepFor<T>(writer.GetInlinedValue(val));
        }
        else {
            // The type is not always inlined, but some values of the type might
            // be if they can be encoded in 4 bytes.
            uint32_t ival = 0;
            if (_EncodeInline(val, &ival)) {
                auto ret = ValueRepFor<T>(ival);
                ret.SetIsInlined();
                return ret;
            }
            
            // Otherwise dedup and/or write...
            if (!_valueDedup) {
                _valueDedup.reset(
                    new typename decltype(_valueDedup)::element_type);
            }
            
            auto iresult = _valueDedup->emplace(val, ValueRep());
            ValueRep &target = iresult.first->second;
            if (iresult.second) {
                // Not yet present.  Invoke the write function.
                target = ValueRepFor<T>(writer.Tell());
                writer.Write(val);
            }
            return target;
        }
    }
    
    template <class Reader>
    inline void Unpack(Reader reader, ValueRep rep, T *out) const {

        if constexpr (_IsAlwaysInlined<T>::value) {
            // Value is directly in payload data.
            uint32_t tmp =
                (rep.GetPayload() & ((1ull << (sizeof(uint32_t) * 8))-1));
            *out = reader.template GetUninlinedValue<T>(tmp);
        }
        else {
            // If the value is inlined, just decode it.
            if (rep.IsInlined()) {
                uint32_t tmp = (rep.GetPayload() &
                                ((1ull << (sizeof(uint32_t) * 8))-1));
                _DecodeInline(out, tmp);
                return;
            }
            // Otherwise we have to read it from the file.
            reader.Seek(rep.GetPayload());
            *out = reader.template Read<T>();
        }
    }

    ValueRep PackArray(_Writer w, VtArray<T> const &array) {
        auto result = ValueRepForArray<T>(0);

        // If this is an empty array we inline it.
        if (array.empty())
            return result;

        if (!_arrayDedup) {
            _arrayDedup.reset(
                new typename decltype(_arrayDedup)::element_type);
        }

        auto iresult = _arrayDedup->emplace(array, result);
        ValueRep &target = iresult.first->second;
        if (iresult.second) {
            // Not yet present.
            if (w.crate->_packCtx->writeVersion < Version(0,5,0)) {
                target.SetPayload(w.Align(sizeof(uint64_t)));
                w.WriteAs<uint32_t>(1);
                w.WriteAs<uint32_t>(array.size());
                w.WriteContiguous(array.cdata(), array.size());
            } else {
                // If we're writing 0.5.0 or greater, see if we can possibly
                // compress this array.
                target = _WritePossiblyCompressedArray(
                    w, array, w.crate->_packCtx->writeVersion, 0);
            }
        }
        return target;
    }

    template <class Reader>
    void UnpackArray(Reader reader, ValueRep rep, VtArray<T> *out) const {
        // If payload is 0, it's an empty array.
        if (rep.GetPayload() == 0) {
            *out = VtArray<T>();
            return;
        }
        reader.Seek(rep.GetPayload());

        // Check version
        Version fileVer(reader.crate->_boot);
        if (fileVer < Version(0,5,0)) {
            // Read and discard shape size.
            reader.template Read<uint32_t>();
        }
        _ReadPossiblyCompressedArray(reader, rep, out, fileVer, 0);
    }

    ValueRep PackVtValue(_Writer w, VtValue const &v) {
        if constexpr (_SupportsArray<T>::value) {
            if (v.IsArrayValued()) {
                return this->PackArray(w, v.UncheckedGet<VtArray<T>>());
            }
        }
        return this->Pack(w, v.UncheckedGet<T>());
    }

    template <class Reader>
    void UnpackVtValue(Reader r, ValueRep rep, VtValue *out) {
        if constexpr (_SupportsArray<T>::value) {
            if (rep.IsArray()) {
                VtArray<T> array;
                this->UnpackArray(r, rep, &array);
                out->Swap(array);
                return;
            }
        }
        T obj;
        this->Unpack(r, rep, &obj);
        out->Swap(obj);
    }
    
    void Clear() {
        if constexpr (!_IsAlwaysInlined<T>::value) {
            _valueDedup.reset();
        }
        if constexpr (_SupportsArray<T>::value) {
            _arrayDedup.reset();
        }                
    }
    
    std::unique_ptr<std::unordered_map<T, ValueRep, _Hasher>> _valueDedup;
    std::unique_ptr<
        std::unordered_map<VtArray<T>, ValueRep, _Hasher>> _arrayDedup;

};

// Don't compress arrays smaller than this.
constexpr size_t MinCompressedArraySize = 16;

template <class Writer, class T>
static inline ValueRep
_WriteUncompressedArray(
    Writer w, VtArray<T> const &array, CrateFile::Version ver)
{
    // We'll align the array to 8 bytes, so software can refer to mapped bytes
    // directly if possible.
    auto result = ValueRepForArray<T>(w.Align(sizeof(uint64_t)));

    (ver < CrateFile::Version(0,7,0)) ?
        w.template WriteAs<uint32_t>(array.size()) :
        w.template WriteAs<uint64_t>(array.size());
    
    w.WriteContiguous(array.cdata(), array.size());

    return result;
}

template <class Writer, class T>
static inline ValueRep
_WritePossiblyCompressedArray(
    Writer w, VtArray<T> const &array, CrateFile::Version ver, ...)
{
    // Fallback case -- write uncompressed data.
    return _WriteUncompressedArray(w, array, ver);
}

template <class Writer, class Int>
static inline void
_WriteCompressedInts(Writer w, Int const *begin, size_t size)
{
    // Make a buffer to compress to, compress, and write.
    using Compressor = typename std::conditional<
        sizeof(Int) == 4,
        Sdf_IntegerCompression,
        Sdf_IntegerCompression64>::type;
    std::unique_ptr<char[]> compBuffer(
        new char[Compressor::GetCompressedBufferSize(size)]);
    size_t compSize =
        Compressor::CompressToBuffer(begin, size, compBuffer.get());
    w.template WriteAs<uint64_t>(compSize);
    w.WriteContiguous(compBuffer.get(), compSize);
}

template <class Writer, class T>
static inline
typename std::enable_if<
    std::is_same<T, int>::value ||
    std::is_same<T, unsigned int>::value ||
    std::is_same<T, int64_t>::value ||
    std::is_same<T, uint64_t>::value,
    ValueRep>::type
_WritePossiblyCompressedArray(
    Writer w, VtArray<T> const &array, CrateFile::Version ver, int)
{
    auto result = ValueRepForArray<T>(w.Tell());
    // Total elements.
    (ver < CrateFile::Version(0,7,0)) ?
        w.template WriteAs<uint32_t>(array.size()) :
        w.template WriteAs<uint64_t>(array.size());
    if (array.size() < MinCompressedArraySize) {
        w.WriteContiguous(array.cdata(), array.size());
    } else {
        _WriteCompressedInts(w, array.cdata(), array.size());
        result.SetIsCompressed();
    }
    return result;
}

template <class Writer, class T>
static inline
typename std::enable_if<
    std::is_same<T, GfHalf>::value ||
    std::is_same<T, float>::value ||
    std::is_same<T, double>::value,
    ValueRep>::type
_WritePossiblyCompressedArray(
    Writer w, VtArray<T> const &array, CrateFile::Version ver, int)
{
    // Version 0.6.0 introduced compressed floating point arrays.
    if (ver < CrateFile::Version(0,6,0) ||
        array.size() < MinCompressedArraySize) {
        return _WriteUncompressedArray(w, array, ver);
    }

    // Check to see if all the floats are exactly represented as integers.
    auto isIntegral = [](T fp) {
        constexpr int32_t max = std::numeric_limits<int32_t>::max();
        constexpr int32_t min = std::numeric_limits<int32_t>::lowest();
        return min <= fp && fp <= max &&
            static_cast<T>(static_cast<int32_t>(fp)) == fp;
    };    
    if (std::all_of(array.cdata(), array.cdata() + array.size(), isIntegral)) {
        // Encode as integers.
        auto result = ValueRepForArray<T>(w.Tell());
        (ver < CrateFile::Version(0,7,0)) ?
            w.template WriteAs<uint32_t>(array.size()) :
            w.template WriteAs<uint64_t>(array.size());
        result.SetIsCompressed();
        vector<int32_t> ints(array.size());
        std::copy(array.cdata(), array.cdata() + array.size(), ints.data());
        // Lowercase 'i' code indicates that the floats are written as
        // compressed ints.
        w.template WriteAs<int8_t>('i');
        _WriteCompressedInts(w, ints.data(), ints.size());
        return result;
    }
    
    // Otherwise check if there are a small number of distinct values, which we
    // can then write as a lookup table and indexes into that table.
    vector<T> lut;
    // Ensure that we give up soon enough if it doesn't seem like building a
    // lookup table will be profitable.  Check the first 1024 elements at most.
    unsigned int maxLutSize = std::min<size_t>(array.size() / 4, 1024);
    vector<uint32_t> indexes;
    for (auto elem: array) {
        auto iter = std::find(lut.begin(), lut.end(), elem);
        uint32_t index = iter-lut.begin();
        indexes.push_back(index);
        if (index == lut.size()) {
            if (lut.size() != maxLutSize) {
                lut.push_back(elem);
            } else {
                lut.clear();
                indexes.clear();
                break;
            }
        }
    }
    if (!lut.empty()) {
        // Use the lookup table.  Lowercase 't' code indicates that floats are
        // written with a lookup table and indexes.
        auto result = ValueRepForArray<T>(w.Tell());
        (ver < CrateFile::Version(0,7,0)) ?
            w.template WriteAs<uint32_t>(array.size()) :
            w.template WriteAs<uint64_t>(array.size());
        result.SetIsCompressed();
        w.template WriteAs<int8_t>('t');
        // Write the lookup table itself.
        w.template WriteAs<uint32_t>(lut.size());
        w.WriteContiguous(lut.data(), lut.size());
        // Now write indexes.
        _WriteCompressedInts(w, indexes.data(), indexes.size());
        return result;
    }

    // Otherwise, just write uncompressed floats.  We don't need to write a code
    // byte here like the 'i' and 't' above since the resulting ValueRep is not
    // marked compressed -- the reader code will thus just read the uncompressed
    // values directly.
    return _WriteUncompressedArray(w, array, ver);
}

template <class Reader, class T>
static inline
typename std::enable_if<!Reader::StreamSupportsZeroCopy ||
                        !_IsBitwiseReadWrite<T>::value>::type
_ReadUncompressedArray(
    Reader reader, ValueRep rep, VtArray<T> *out, CrateFile::Version ver)
{
    // The reader's bytestream does not support zero-copy, or the element type
    // is not bitwise identical in memory and on disk, so just read the contents
    // into memory.
    out->resize(
        ver < CrateFile::Version(0,7,0) ?
        reader.template Read<uint32_t>() :
        reader.template Read<uint64_t>());
    reader.ReadContiguous(out->data(), out->size());
}

template <class Reader, class T>
static inline
typename std::enable_if<Reader::StreamSupportsZeroCopy &&
                        _IsBitwiseReadWrite<T>::value>::type
_ReadUncompressedArray(
    Reader reader, ValueRep rep, VtArray<T> *out, CrateFile::Version ver)
{
    static bool zeroCopyEnabled = TfGetEnvSetting(USDC_ENABLE_ZERO_COPY_ARRAYS);
    
    // The reader's stream supports zero-copy and T is written to disk just as
    // it is represented in memory, so if the array is of reasonable size and
    // the memory is suitably aligned, then make an array that refers directly
    // into the stream's memory.

    uint64_t size = (ver < CrateFile::Version(0,7,0)) ?
        reader.template Read<uint32_t>() :
        reader.template Read<uint64_t>();

    // Check size and alignment -- the standard requires that alignments
    // are power-of-two.
    size_t numBytes = sizeof(T) * size;
    static constexpr size_t MinZeroCopyArrayBytes = 2048; // Half a page?
    if (zeroCopyEnabled &&
        /* size reasonable? */numBytes >= MinZeroCopyArrayBytes &&
        /*    alignment ok? */
        (reinterpret_cast<uintptr_t>(
            reader.src.TellMemoryAddress()) & (alignof(T)-1)) == 0) {

        // Make a VtArray with a foreign source that points into the stream.  We
        // pass addRef=false here, because CreateZeroCopyDataSource does that
        // already -- it needs to know if it's taken the count from 0 to 1 or
        // not.
        void const *addr = reader.src.TellMemoryAddress();

        if (Vt_ArrayForeignDataSource *foreignSrc =
            reader.src.CreateZeroCopyDataSource(addr, numBytes)) {
            *out = VtArray<T>(
                foreignSrc,
                static_cast<T *>(const_cast<void *>(addr)),
                size, /*addRef=*/false);
        }
        else {
            // In case of error, return an empty array.
            out->clear();
        }
    }
    else {
        // Copy the data instead.
        out->resize(size);
        reader.ReadContiguous(out->data(), out->size());
    }
}

template <class Reader, class T>
static inline void
_ReadPossiblyCompressedArray(
    Reader reader, ValueRep rep, VtArray<T> *out, CrateFile::Version ver, ...)
{
    // Fallback uncompressed case.
    _ReadUncompressedArray(reader, rep, out, ver);
}

struct _CompressedIntsReader
{
    template <class Reader, class Int>
    void Read(Reader &reader, Int *out, size_t numInts) {
        using Compressor = typename std::conditional<
            sizeof(Int) == 4,
            Sdf_IntegerCompression,
            Sdf_IntegerCompression64>::type;
        
        _AllocateBufferAndWorkingSpace<Compressor>(numInts);
        auto compressedSize = reader.template Read<uint64_t>();
        if (compressedSize > _compBufferSize) {
            // Don't read more than the available memory buffer.
            compressedSize = _compBufferSize;
        }
        reader.ReadContiguous(_compBuffer.get(), compressedSize);
        Compressor::DecompressFromBuffer(
            _compBuffer.get(), compressedSize, out, numInts,
            _workingSpace.get());
    }

private:
    template <class Comp>
    void _AllocateBufferAndWorkingSpace(size_t numInts) {
        size_t reqBufferSize = Comp::GetCompressedBufferSize(numInts);
        size_t reqWorkingSize = Comp::GetDecompressionWorkingSpaceSize(numInts);
        if (reqBufferSize > _compBufferSize) {
            _compBuffer.reset(new char[reqBufferSize]);
            _compBufferSize = reqBufferSize;
        }
        if (reqWorkingSize > _workingSpaceSize) {
            _workingSpace.reset(new char[reqWorkingSize]);
            _workingSpaceSize = reqWorkingSize;
        }
    }
    
    std::unique_ptr<char[]> _compBuffer;
    size_t _compBufferSize = 0;
    std::unique_ptr<char[]> _workingSpace;
    size_t _workingSpaceSize = 0;
};

template <class Reader, class Int>
static inline void
_ReadCompressedInts(Reader &reader, Int *out, size_t size)
{
    _CompressedIntsReader r;
    r.Read(reader, out, size);
}

template <class Reader, class T>
static inline
typename std::enable_if<
    std::is_same<T, int>::value ||
    std::is_same<T, unsigned int>::value ||
    std::is_same<T, int64_t>::value ||
    std::is_same<T, uint64_t>::value>::type
_ReadPossiblyCompressedArray(
    Reader reader, ValueRep rep, VtArray<T> *out, CrateFile::Version ver, int)
{
    // Version 0.5.0 introduced compressed int arrays.
    if (ver < CrateFile::Version(0,5,0) || !rep.IsCompressed()) {
        _ReadUncompressedArray(reader, rep, out, ver);
    }
    else {
        // Read total elements.
        out->resize(ver < CrateFile::Version(0,7,0) ?
                    reader.template Read<uint32_t>() :
                    reader.template Read<uint64_t>());
        if (out->size() < MinCompressedArraySize) {
            reader.ReadContiguous(out->data(), out->size());
        } else {
            _ReadCompressedInts(reader, out->data(), out->size());
        }
    }
}

template <class Reader, class T>
static inline
typename std::enable_if<
    std::is_same<T, GfHalf>::value ||
    std::is_same<T, float>::value ||
    std::is_same<T, double>::value>::type
_ReadPossiblyCompressedArray(
    Reader reader, ValueRep rep, VtArray<T> *out, CrateFile::Version ver, int)
{
    // Version 0.6.0 introduced compressed floating point arrays.
    if (ver < CrateFile::Version(0,6,0) || !rep.IsCompressed()) {
        _ReadUncompressedArray(reader, rep, out, ver);
        return;
    }

    out->resize(ver < CrateFile::Version(0,7,0) ?
                reader.template Read<uint32_t>() :
                reader.template Read<uint64_t>());
    auto odata = out->data();
    auto osize = out->size();

    if (osize < MinCompressedArraySize) {
        // Not stored compressed.
        reader.ReadContiguous(odata, osize);
        return;
    }
    
    // Read the code
    char code = reader.template Read<int8_t>();
    if (code == 'i') {
        // Compressed integers.
        vector<int32_t> ints(osize);
        _ReadCompressedInts(reader, ints.data(), ints.size());
        std::copy(ints.begin(), ints.end(), odata);
    } else if (code == 't') {
        // Lookup table & indexes.
        auto lutSize = reader.template Read<uint32_t>();
        vector<T> lut(lutSize);
        reader.ReadContiguous(lut.data(), lut.size());
        vector<uint32_t> indexes(osize);
        _ReadCompressedInts(reader, indexes.data(), indexes.size());
        auto o = odata;
        for (auto index: indexes) {
            *o++ = lut[index];
        }
    } else {
        // This is a corrupt data stream.
        TF_RUNTIME_ERROR("Corrupt data stream detected reading compressed "
                         "array in <%s>", reader.crate->GetAssetPath().c_str());
    }
}

////////////////////////////////////////////////////////////////////////
// CrateFile

/*static*/
bool
CrateFile::CanRead(string const &assetPath)
{
    // Fetch the asset from Ar.
    auto asset = ArGetResolver().OpenAsset(ArResolvedPath(assetPath));
    return asset && CanRead(assetPath, asset);
}

/*static*/
bool
CrateFile::CanRead(string const &assetPath, ArAssetSharedPtr const &asset)
{
    // If the asset has a file, mark it random access to avoid prefetch.
    FILE *file; size_t offset;
    std::tie(file, offset) = asset->GetFileUnsafe();
    if (file) {
        ArchFileAdvise(file, offset, asset->GetSize(),
                       ArchFileAdviceRandomAccess);
    }

    TfErrorMark m;
    _ReadBootStrap(_AssetStream(asset), asset->GetSize());

    // Clear any issued errors again to avoid propagation, and return true if
    // there were no errors issued.
    bool canRead = !m.Clear();

    // Restore prefetching behavior to "normal".
    if (file) {
        ArchFileAdvise(file, offset, asset->GetSize(), ArchFileAdviceNormal);
    }

    return canRead;
}

/* static */
std::unique_ptr<CrateFile>
CrateFile::CreateNew(bool detached)
{
    const bool useMmap = 
        !TfGetEnvSetting(USDC_USE_ASSET) &&
        !TfGetenvBool("USDC_USE_PREAD", false);

    const Options opt = 
        detached ? Options::Detached :
        useMmap ? Options::UseMmap :
        Options::Default;
    
    return std::unique_ptr<CrateFile>(new CrateFile(opt));
}

/* static */
CrateFile::_FileMapping
CrateFile::_MmapAsset(char const *assetPath, ArAssetSharedPtr const &asset)
{
    FILE *file; size_t offset;
    std::tie(file, offset) = asset->GetFileUnsafe();
    std::string errMsg;
    auto mapping = _FileMapping(ArchMapFileReadOnly(file, &errMsg),
                                offset, asset->GetSize());
    if (!mapping.GetMapStart()) {
        TF_RUNTIME_ERROR("Couldn't map asset '%s'%s%s", assetPath,
                         !errMsg.empty() ? ": " : "",
                         errMsg.c_str());
        mapping.Reset();
    }
    return mapping;
}

/* static */
CrateFile::_FileMapping
CrateFile::_MmapFile(char const *fileName, FILE *file)
{
    std::string errMsg;
    auto mapping = _FileMapping(ArchMapFileReadOnly(file, &errMsg));
    if (!mapping.GetMapStart()) {
        TF_RUNTIME_ERROR("Couldn't map file '%s'%s%s", fileName,
                         !errMsg.empty() ? ": " : "",
                         errMsg.c_str());
        mapping.Reset();
    }
    return mapping;
}

/* static */
std::unique_ptr<CrateFile>
CrateFile::Open(string const &assetPath,
                bool detached)
{
    TfAutoMallocTag tag("Sdf_CrateFile::CrateFile::Open");
    return Open(
        assetPath, ArGetResolver().OpenAsset(ArResolvedPath(assetPath)),
        detached);
}

std::unique_ptr<CrateFile>
CrateFile::Open(string const &assetPath, ArAssetSharedPtr const &srcAsset,
                bool detached)
{
    TfAutoMallocTag tag("Sdf_CrateFile::CrateFile::Open");

    std::unique_ptr<CrateFile> result;

    ArAssetSharedPtr detachedAsset;
    if (detached && srcAsset) {
        detachedAsset = srcAsset->GetDetachedAsset();
    }

    const ArAssetSharedPtr& asset = detached ? detachedAsset : srcAsset;

    if (!asset) {
        TF_RUNTIME_ERROR("Failed to open asset '%s'", assetPath.c_str());
        return result;
    }

    if (!TfGetEnvSetting(USDC_USE_ASSET)) {
        // See if we can get an underlying FILE * for the asset.
        FILE *file; size_t offset;
        std::tie(file, offset) = asset->GetFileUnsafe();
        if (file) {
            // If so, then we'll either mmap it or use pread() on it.
            if (!TfGetenvBool("USDC_USE_PREAD", false)) {
                // Try to memory-map the file.
                auto mapping = _MmapAsset(assetPath.c_str(), asset);
                result.reset(new CrateFile(assetPath, ArchGetFileName(file),
                                           std::move(mapping), asset));
            } else {
                // Use pread with the asset's file.
                result.reset(new CrateFile(
                                 assetPath, ArchGetFileName(file),
                                 _FileRange(
                                     file, offset, asset->GetSize(),
                                     /*hasOwnership=*/ false),
                                 asset));
            }
        }
    }

    if (!result) {
        // With no underlying FILE *, we'll go through ArAsset::Read() directly.
        result.reset(new CrateFile(assetPath, asset, detached));
    }
    
    // If the resulting CrateFile has no asset path, reading failed.
    if (result->GetAssetPath().empty())
        result.reset();
    
   return result;
}

/* static */
CrateFile::Version
CrateFile::GetSoftwareVersion()
{
    return _SoftwareVersion;
}

/* static */
TfToken const &
CrateFile::GetSoftwareVersionToken()
{
    static TfToken tok(GetSoftwareVersion().AsString());
    return tok;
}

CrateFile::Version
CrateFile::GetFileVersion() const
{
    return Version(_boot);
}

TfToken
CrateFile::GetFileVersionToken() const
{
    return TfToken(Version(_boot).AsString());
}

CrateFile::CrateFile(Options opt)
    : _detached(opt == Options::Detached)
    , _useMmap(opt == Options::UseMmap)
{
    _DoAllTypeRegistrations();
}

CrateFile::CrateFile(string const &assetPath, string const &fileName,
                     _FileMapping &&mapping, ArAssetSharedPtr const &asset)
    : _mmapSrc(std::move(mapping))
    , _detached(false)
    , _assetPath(assetPath)
    , _useMmap(true)
{
    // Note that we intentionally do not store the asset -- we want to close the
    // file handle if possible.
    _DoAllTypeRegistrations();
    _InitMMap();
}

void
CrateFile::_InitMMap() {
    if (_mmapSrc) {
        int64_t mapSize = _mmapSrc.GetLength();
        
        // Mark the whole file as random access to start to avoid large NFS
        // prefetch.  We explicitly prefetch the structural sections later.
        ArchMemAdvise(
            _mmapSrc.GetMapStart(), mapSize, ArchMemAdviceRandomAccess);

        // If we're debugging access, allocate a debug page map. 
        static string debugPageMapPattern = TfGetenv("USDC_DUMP_PAGE_MAPS");
        // If it's just '1' or '*' do everything, otherwise match.
        if (!debugPageMapPattern.empty() &&
            (debugPageMapPattern == "*" || debugPageMapPattern == "1" ||
             ArchRegex(debugPageMapPattern,
                       ArchRegex::GLOB).Match(_assetPath))) {
            auto pageAlignedMapSize =
                (_mmapSrc.GetMapStart() + mapSize) -
                RoundToPageAddr(_mmapSrc.GetMapStart());
            int64_t npages =
                (pageAlignedMapSize + CRATE_PAGESIZE-1) / CRATE_PAGESIZE;
            _debugPageMap.reset(new char[npages]);
            memset(_debugPageMap.get(), 0, npages);
        } 

        // Make an mmap stream but disable auto prefetching -- the
        // _ReadStructuralSections() call manages prefetching itself using
        // higher-level knowledge.
        auto reader = _MakeReader(
            _MakeMmapStream(&_mmapSrc, _debugPageMap.get()).DisablePrefetch());
        TfErrorMark m;
        _ReadStructuralSections(reader, mapSize);
        if (!m.IsClean()){
            _assetPath.clear();
        }

        // Restore default prefetch behavior if we're not doing custom prefetch.
        if (!_GetMMapPrefetchKB()) {
            ArchMemAdvise(
                _mmapSrc.GetMapStart(), mapSize, ArchMemAdviceNormal);
        }
    } else {
        _assetPath.clear();
    }
}

CrateFile::CrateFile(string const &assetPath, string const &fileName,
                     _FileRange &&inputFile, ArAssetSharedPtr const &asset)
    : _preadSrc(std::move(inputFile))
    , _assetSrc(asset)
    , _detached(false)
    , _assetPath(assetPath)
    , _useMmap(false)
{
    // Note that we *do* store the asset here, since we need to keep the FILE*
    // alive to pread from it.
    _DoAllTypeRegistrations();
    _InitPread();
}

void
CrateFile::_InitPread()
{
    // Mark the whole file range as random access to start to avoid large NFS
    // prefetch.  We explicitly prefetch the structural sections later.
    int64_t rangeLength = _preadSrc.GetLength();
    ArchFileAdvise(_preadSrc.file, _preadSrc.startOffset,
                   rangeLength, ArchFileAdviceRandomAccess);
    auto reader = _MakeReader(_PreadStream(_preadSrc));
    TfErrorMark m;
    _ReadStructuralSections(reader, rangeLength);
    if (!m.IsClean()) {
        _assetPath.clear();
    }
    // Restore default prefetch behavior.
    ArchFileAdvise(_preadSrc.file, _preadSrc.startOffset,
                   rangeLength, ArchFileAdviceNormal);
}

CrateFile::CrateFile(string const &assetPath, ArAssetSharedPtr const &asset,
                     bool detached)
    : _assetSrc(asset)
    , _detached(detached)
    , _assetPath(assetPath)
    , _useMmap(false)
{
    _DoAllTypeRegistrations();
    _InitAsset();
}

void
CrateFile::_InitAsset()
{
    auto reader = _MakeReader(_AssetStream(_assetSrc));
    TfErrorMark m;
    _ReadStructuralSections(reader, _assetSrc->GetSize());
    if (!m.IsClean())
        _assetPath.clear();
}

CrateFile::~CrateFile()
{
    static std::mutex outputMutex;

    // Dump a debug page map if requested.
    if (_useMmap && _mmapSrc && _debugPageMap) {
        auto mapStart = _mmapSrc.GetMapStart();
        int64_t startPage = GetPageNumber(mapStart);
        int64_t endPage = GetPageNumber(mapStart + _mmapSrc.GetLength() - 1);
        int64_t npages = 1 + endPage - startPage;
        std::unique_ptr<unsigned char []> mincoreMap(new unsigned char[npages]);
        void const *p = static_cast<void const *>(RoundToPageAddr(mapStart));
        if (!ArchQueryMappedMemoryResidency(
                p, npages*CRATE_PAGESIZE, mincoreMap.get())) {
            TF_WARN("failed to obtain memory residency information");
            return;
        }
        // Count the pages in core & accessed.
        int64_t pagesInCore = 0;
        int64_t pagesAccessed = 0;
        for (int64_t i = 0; i != npages; ++i) {
            bool inCore = mincoreMap[i] & 1;
            bool accessed = _debugPageMap[i] & 1;
            pagesInCore += (int)inCore;
            pagesAccessed += (int)accessed;
            if (accessed && inCore) {
                mincoreMap.get()[i] = '+';
            } else if (accessed) {
                mincoreMap.get()[i] = '!';
            } else if (inCore) {
                mincoreMap.get()[i] = '-';
            } else {
                mincoreMap.get()[i] = ' ';
            }
        }

        std::lock_guard<std::mutex> lock(outputMutex);

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
               ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"
               "page map for %s\n"
               "%" PRId64 " pages, %" PRId64 " used (%.1f%%), %" PRId64
               " in mem (%.1f%%)\n"
               "used %.1f%% of pages in mem\n"
               "legend: '+': in mem & used,     '-': in mem & unused\n"
               "        '!': not in mem & used, ' ': not in mem & unused\n"
               ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
               ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n",
               _assetPath.c_str(),
               npages,
               pagesAccessed, 100.0*pagesAccessed/(double)npages,
               pagesInCore, 100.0*pagesInCore/(double)npages,
               100.0*pagesAccessed / (double)pagesInCore);
               
        constexpr int wrapCol = 80;
        int col = 0;
        for (int64_t i = 0; i != npages; ++i, ++col) {
            putchar(mincoreMap.get()[i]);
            if (col == wrapCol) {
                putchar('\n');
                col = -1;
            }
        }
        printf("\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
               "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    }

    // If we have zero copy ranges to detach, do it now.
    if (_useMmap && _mmapSrc) {
        _mmapSrc.Reset();
    }

    WorkMoveDestroyAsync(_paths);
    WorkMoveDestroyAsync(_tokens);
    WorkMoveDestroyAsync(_strings);
    WorkMoveDestroyAsync(_sharedTimes);
    WorkMoveDestroyAsync(_packValueFunctions);

    _DeleteValueHandlers();
}

CrateFile::Packer
CrateFile::StartPacking(string const &fileName)
{
    auto out = ArGetResolver().OpenAssetForWrite(
        ArResolvedPath(fileName), 
        _assetPath.empty() ? 
            ArResolver::WriteMode::Replace :
            ArResolver::WriteMode::Update);
    if (!out) {
        TF_RUNTIME_ERROR("Unable to open %s for write", fileName.c_str());
    }
    else {
        // Create a packing context so we can start writing.
        _packCtx.reset(new _PackingContext(this, std::move(out), fileName));
        // Get rid of our local list of specs, if we have one -- the client is
        // required to repopulate it.
        vector<Spec>().swap(_specs);
        // If we have no tokens yet, insert a special token that cannot be used
        // as a prim property path element so that it gets token index 0.
        // There's a bug (github issue 811) in the compressed path code where it
        // uses negative indexes to indicate prim property path elements.  That
        // fails for index 0.  So inserting a token here that cannot be used as
        // a property path element sidesteps this.
        if (_tokens.empty()) {
            _AddToken(TfToken(";-)"));
        }
    }
    return Packer(this);
}

CrateFile::Packer::operator bool() const {
    return _crate && _crate->_packCtx;
}

bool
CrateFile::Packer::Close()
{
    if (!TF_VERIFY(_crate && _crate->_packCtx))
        return false;

    // Write contents. Always close the output asset even if writing failed.
    bool writeResult = _crate->_Write();

    if (writeResult) {
        // Abandon the asset here to release resources that the subsequent call
        // to CloseOutputAsset() might need.  E.g. on Windows,
        // CloseOutputAsset() may try to overwrite the file that _assetSrc has
        // open for read.
        _crate->_assetSrc.reset();
    }
    
    writeResult &= _crate->_packCtx->CloseOutputAsset();
    
    // If we wrote successfully, store the fileName.
    if (writeResult) {
        _crate->_assetPath = _crate->_packCtx->fileName;
    }

    _crate->_packCtx.reset();

    if (!writeResult)
        return false;

    // Reset so we can read values from the newly written asset.
    // See CrateFile::Open.
    auto asset = ArGetResolver().OpenAsset(ArResolvedPath(_crate->_assetPath));
    if (asset && _crate->IsDetached()) {
        asset = asset->GetDetachedAsset();
    }

    if (!asset) {
        return false;
    }

    if (!TfGetEnvSetting(USDC_USE_ASSET)) {
        FILE *file; size_t offset;
        std::tie(file, offset) = asset->GetFileUnsafe();
        if (file) {
            if (_crate->_useMmap) {
                // Must remap the file.
                _crate->_mmapSrc = _MmapFile(_crate->_assetPath.c_str(), file);
                if (!_crate->_mmapSrc) {
                    return false;
                }
                _crate->_assetSrc.reset();
                _crate->_InitMMap();
            }
            else {
                _crate->_preadSrc = _FileRange(
                    file, offset, asset->GetSize(), /*hasOwnership=*/false);
                _crate->_assetSrc = asset;
                _crate->_InitPread();
            }

            return true;
        }
    }

    _crate->_mmapSrc.Reset();
    _crate->_preadSrc = _FileRange();
    _crate->_assetSrc = asset;
    _crate->_InitAsset();
        
    return true;
}

CrateFile::Packer::Packer(Packer &&other) : _crate(other._crate)
{
    other._crate = nullptr;
}

CrateFile::Packer &
CrateFile::Packer::operator=(Packer &&other)
{
    _crate = other._crate;
    other._crate = nullptr;
    return *this;
}

CrateFile::Packer::~Packer()
{
    if (_crate) {
        _crate->_packCtx.reset();
    }
}

vector<tuple<string, int64_t, int64_t>>
CrateFile::GetSectionsNameStartSize() const
{
    vector<tuple<string, int64_t, int64_t> > result;
    for (auto const &sec: _toc.sections) {
        result.emplace_back(sec.name, sec.start, sec.size);
    }
    return result;
}

template <class Fn>
void
CrateFile::_WriteSection(
    _Writer &w, _SectionName name, _TableOfContents &toc, Fn writeFn) const
{
    toc.sections.emplace_back(name.c_str(), w.Tell(), 0);
    writeFn();
    toc.sections.back().size = w.Tell() - toc.sections.back().start;
}

void
CrateFile::_AddDeferredSpecs()
{
    // A map from sample time to VtValues within TimeSamples instances in
    // _deferredSpecs.
    pxr_tsl::robin_map<double, vector<VtValue *>, TfHash> allValuesAtAllTimes;

    // Search for the TimeSamples, add to the allValuesAtAllTimes.
    for (auto &spec: _deferredSpecs) {
        for (auto &tsf: spec.timeSampleFields) {
            for (size_t i = 0; i != tsf.second.values.size(); ++i) {
                if (!tsf.second.values[i].IsHolding<ValueRep>()) {
                    allValuesAtAllTimes[tsf.second.times.Get()[i]].push_back(
                        &tsf.second.values[i]);
                }
            }
        }
    }

    // Create a sorted view of the underlying map keys.
    std::vector<double> orderedTimes(allValuesAtAllTimes.size());
    std::transform(std::cbegin(allValuesAtAllTimes),
                   std::cend(allValuesAtAllTimes),
                   std::begin(orderedTimes),
                   [](const auto& element) { return element.first; });
    std::sort(orderedTimes.begin(), orderedTimes.end());

    // Now walk through allValuesAtAllTimes in order and pack all the values,
    // swapping them out with the resulting reps.  This ensures that when we
    // pack the specs, which will re-pack the values, they'll be noops since
    // they are just holding value reps that point into the file.
    for (auto const &t: orderedTimes) {
        auto it = allValuesAtAllTimes.find(t);
        TF_DEV_AXIOM(it != allValuesAtAllTimes.end());
        for (VtValue *val: it->second)
            *val = _PackValue(*val);
    }

    // Now we've transformed all the VtValues in all the timeSampleFields to
    // ValueReps.  We can call _AddField and add them (and any of the other
    // deferred fields) to ordinaryFields, then add the spec.
    for (auto &spec: _deferredSpecs) {
        // Add the deferred ordinary fields
        for (auto &fv: spec.deferredOrdinaryFields) {
            spec.ordinaryFields.push_back(_AddField(fv));
        }
        // Add the deferred time sample fields
        for (auto &p: spec.timeSampleFields) {
            spec.ordinaryFields.push_back(
                _AddField(make_pair(p.first, VtValue::Take(p.second))));
        }
        _specs.emplace_back(spec.path, spec.specType,
                            _AddFieldSet(spec.ordinaryFields));
    }

    TfReset(_deferredSpecs);
}

bool
CrateFile::_Write()
{
    // First, add any _deferredSpecs, including packing time sample field values
    // time-by-time to ensure that all the data for given times is collocated.
    _AddDeferredSpecs();

    // Now proceed with writing.
    _Writer w(this);

    _TableOfContents toc;

    // Write out the sections we don't know about that the packing context
    // captured.
    using std::get;
    for (auto const &s: _packCtx->unknownSections) {
        _Section sec(get<0>(s).c_str(), w.Tell(), get<2>(s));
        w.WriteContiguous(get<1>(s).get(), sec.size);
        toc.sections.push_back(sec);
    }

    _WriteSection(w, _TokensSectionName, toc, [this, &w]() {_WriteTokens(w);});
    _WriteSection(
        w, _StringsSectionName, toc, [this, &w]() {w.Write(_strings);});
    _WriteSection(w, _FieldsSectionName, toc, [this, &w]() {_WriteFields(w);});
    _WriteSection(
        w, _FieldSetsSectionName, toc, [this, &w]() {_WriteFieldSets(w);});
    _WriteSection(w, _PathsSectionName, toc, [this, &w]() {_WritePaths(w);});
    _WriteSection(w, _SpecsSectionName, toc, [this, &w]() {_WriteSpecs(w);});

    _BootStrap boot(_packCtx->writeVersion);

    // Record TOC location, and write it.
    boot.tocOffset = w.Tell();
    w.Write(toc);

    // Write bootstrap at start of file.
    w.Seek(0);
    w.Write(boot);

    // Flush any buffered writes.
    w.Flush();

    _toc = toc;
    _boot = boot;

    // Clear dedup tables.
    _ClearValueHandlerDedupTables();

    return true;
}

void
CrateFile::_AddSpec(const SdfPath &path, SdfSpecType type,
                    const std::vector<FieldValuePair> &fields) 
{
    vector<FieldIndex> ordinaryFields; // non time-sample valued fields.
    vector<pair<TfToken, TimeSamples>> timeSampleFields;
    vector<FieldValuePair> versionUpgradePendingFields;

    auto _IsCompatiblePre08PayloadValue = [this](const VtValue &v)
    {
        // There are two cases where a field's value is backwards compatible
        // with verion 0.7.0. 
        // 1. The value holds an SdfPayload with an identity layer offset.
        // 2. The value holds a ValueRep that packs a payload read from a 0.7.0
        //    or earlier crate file.
        // In both cases, the value will need to be repacked if the version
        // needs to be upgraded to 0.8.0 or higher for any reason.
        return (v.IsHolding<SdfPayload>() && 
                v.UncheckedGet<SdfPayload>().GetLayerOffset().IsIdentity()) ||
            (Version(this->_boot) < Version(0, 8, 0) &&
             v.IsHolding<ValueRep>() &&
             v.UncheckedGet<ValueRep>().GetType() == TypeEnum::Payload);
    };

    ordinaryFields.reserve(fields.size());
    for (auto const &p: fields) {
        if (p.second.IsHolding<TimeSamples>() &&
            p.second.UncheckedGet<TimeSamples>().IsInMemory()) {
            // If any of the fields here are TimeSamples, then defer adding 
            // this spec to the call to _Write().  In _Write(), we'll add all 
            // the sample values time-by-time to ensure that all the data for a 
            // given sample time is as collocated as possible in the file.
            timeSampleFields.emplace_back(
                p.first, p.second.UncheckedGet<TimeSamples>());
        } else if (_packCtx->writeVersion < Version(0, 8, 0) &&
                   _IsCompatiblePre08PayloadValue(p.second)) {
            // If the file we're writing has not yet been upgraded to a 0.8.0 or 
            // later version and the field value is a SdfPayload that can still
            // be represented in a older version, then we defer this spec until 
            // the call to _Write. This is to make sure that if we end up 
            // needing to upgrade the file version for some other field or spec,
            // that we still write all SdfPayloads in the file using the current
            // format instead of having a mix of formats depending on the order 
            // we wrote our payload values in.
            versionUpgradePendingFields.push_back(p);
        } else if (p.second.IsHolding<TsSpline>()
            && p.second.UncheckedGet<TsSpline>().IsEmpty()) {
            // Don't serialize empty splines, because they don't affect
            // anything.
        } else {
            ordinaryFields.push_back(_AddField(p));
        }
    }

    // If we have no time sample fields or version upgrade pending fields, we
    // can just add the spec now. Otherwise defer this spec until _Write().
    if (timeSampleFields.empty() && versionUpgradePendingFields.empty()) {
        _specs.emplace_back(_AddPath(path), type, _AddFieldSet(ordinaryFields));
    }
    else {
        _deferredSpecs.emplace_back(
            _AddPath(path), type,
            std::move(ordinaryFields), 
            std::move(versionUpgradePendingFields),
            std::move(timeSampleFields));
    }        
}

VtValue
CrateFile::_GetTimeSampleValueImpl(TimeSamples const &ts, size_t i) const
{
    // Need to read the rep from the file for index i.
    auto offset = ts.valuesFileOffset + i * sizeof(ValueRep);
    if (_useMmap) {
        auto reader =
            _MakeReader(_MakeMmapStream(&_mmapSrc, _debugPageMap.get()));
        reader.Seek(offset);
        return VtValue(reader.Read<ValueRep>());
    } else if (_preadSrc) {
        auto reader = _MakeReader(_PreadStream(_preadSrc));
        reader.Seek(offset);
        return VtValue(reader.Read<ValueRep>());
    } else {
        auto reader = _MakeReader(_AssetStream(_assetSrc));
        reader.Seek(offset);
        return VtValue(reader.Read<ValueRep>());
    }
}

void
CrateFile::_MakeTimeSampleValuesMutableImpl(TimeSamples &ts) const
{
    // Read out the reps into the vector.
    ts.values.resize(ts.times.Get().size());
    if (_useMmap) {
        auto reader =
            _MakeReader(_MakeMmapStream(&_mmapSrc, _debugPageMap.get()));
        reader.Seek(ts.valuesFileOffset);
        for (size_t i = 0, n = ts.times.Get().size(); i != n; ++i)
            ts.values[i] = reader.Read<ValueRep>();
    } else if (_preadSrc) {
        auto reader = _MakeReader(_PreadStream(_preadSrc));
        reader.Seek(ts.valuesFileOffset);
        for (size_t i = 0, n = ts.times.Get().size(); i != n; ++i)
            ts.values[i] = reader.Read<ValueRep>();
    } else {
        auto reader = _MakeReader(_AssetStream(_assetSrc));
        reader.Seek(ts.valuesFileOffset);
        for (size_t i = 0, n = ts.times.Get().size(); i != n; ++i)
            ts.values[i] = reader.Read<ValueRep>();
    }
    // Now in memory, no longer reading everything from file.
    ts.valueRep = ValueRep(0);
}

void
CrateFile::_WriteFields(_Writer &w)
{
    if (_packCtx->writeVersion < Version(0,4,0)) {
        // Old-style uncompressed fields.
        w.Write(_fields);
    } else {
        // Compressed fields in 0.4.0.

        // Total # of fields.
        w.WriteAs<uint64_t>(_fields.size());

        // Token index values.
        vector<uint32_t> tokenIndexVals(_fields.size());
        std::transform(_fields.begin(), _fields.end(),
                       tokenIndexVals.begin(),
                       [](Field const &f) { return f.tokenIndex.value; });
        std::unique_ptr<char[]> compBuffer(
            new char[Sdf_IntegerCompression::
                     GetCompressedBufferSize(tokenIndexVals.size())]);

        size_t tokenIndexesSize = Sdf_IntegerCompression::CompressToBuffer(
            tokenIndexVals.data(), tokenIndexVals.size(), compBuffer.get());
        w.WriteAs<uint64_t>(tokenIndexesSize);
        w.WriteContiguous(compBuffer.get(), tokenIndexesSize);

        // ValueReps.
        vector<uint64_t> reps(_fields.size());
        std::transform(_fields.begin(), _fields.end(),
                       reps.begin(),
                       [](Field const &f) { return f.valueRep.data; });

        std::unique_ptr<char[]> compBuffer2(
            new char[TfFastCompression::
                     GetCompressedBufferSize(reps.size() * sizeof(reps[0]))]);
        uint64_t repsSize = TfFastCompression::CompressToBuffer(
            reinterpret_cast<char *>(reps.data()),
            compBuffer2.get(), reps.size() * sizeof(reps[0]));
        w.WriteAs<uint64_t>(repsSize);
        w.WriteContiguous(compBuffer2.get(), repsSize);
    }
}

void
CrateFile::_WriteFieldSets(_Writer &w)
{
    if (_packCtx->writeVersion < Version(0,4,0)) {
        // Old-style uncompressed fieldSets.
        w.Write(_fieldSets);
    } else {
        // Compressed fieldSets.
        vector<uint32_t> fieldSetsVals(_fieldSets.size());
        std::transform(_fieldSets.begin(), _fieldSets.end(),
                       fieldSetsVals.begin(),
                       [](FieldIndex fi) { return fi.value; });
        std::unique_ptr<char[]> compBuffer(
            new char[Sdf_IntegerCompression::
                     GetCompressedBufferSize(fieldSetsVals.size())]);
        // Total # of fieldSetVals.
        w.WriteAs<uint64_t>(fieldSetsVals.size());

        size_t fsetsSize = Sdf_IntegerCompression::CompressToBuffer(
            fieldSetsVals.data(), fieldSetsVals.size(), compBuffer.get());
        w.WriteAs<uint64_t>(fsetsSize);
        w.WriteContiguous(compBuffer.get(), fsetsSize);
    }
}

void
CrateFile::_WritePaths(_Writer &w)
{
    // Write the total # of paths.
    w.WriteAs<uint64_t>(_paths.size());

    if (_packCtx->writeVersion < Version(0,4,0)) {
        // Old-style uncompressed paths.
        SdfPathTable<PathIndex> pathToIndexTable;
        for (auto const &item: _packCtx->pathToPathIndex)
            pathToIndexTable[item.first] = item.second;
        _WritePathTree(w, pathToIndexTable.begin(), pathToIndexTable.end());
        WorkSwapDestroyAsync(pathToIndexTable);
    } else {
        // Write compressed paths.
        vector<pair<SdfPath, PathIndex>> ppaths;
        ppaths.reserve(_paths.size());
        for (auto const &p: _paths) {
            if (!p.IsEmpty()) {
                ppaths.emplace_back(p, _packCtx->pathToPathIndex[p]);
            }
        }
        std::sort(ppaths.begin(), ppaths.end(),
                  [](pair<SdfPath, PathIndex> const &l,
                     pair<SdfPath, PathIndex> const &r) {
                      return l.first < r.first;
                  });
        _WriteCompressedPathData(w, ppaths);
    }
}

void
CrateFile::_WriteSpecs(_Writer &w)
{
    // VERSIONING: If we're writing version 0.0.1, we need to convert to the old
    // form.
    if (_packCtx->writeVersion == Version(0,0,1)) {
        // Copy and write old-structure specs.
        vector<Spec_0_0_1> old(_specs.begin(), _specs.end());
        w.Write(old);
    } else if (_packCtx->writeVersion < Version(0,4,0)) {
        w.Write(_specs);
    } else {
        // Version 0.4.0 introduces compressed specs.  We write three lists of
        // integers here, pathIndexes, fieldSetIndexes, specTypes.
        std::unique_ptr<char[]> compBuffer(
            new char[Sdf_IntegerCompression::
                     GetCompressedBufferSize(_specs.size())]);
        vector<uint32_t> tmp(_specs.size());
        
        // Total # of specs.
        w.WriteAs<uint64_t>(_specs.size());
        
        // pathIndexes.
        std::transform(_specs.begin(), _specs.end(), tmp.begin(),
                       [](Spec const &s) { return s.pathIndex.value; });
        size_t pathIndexesSize = Sdf_IntegerCompression::CompressToBuffer(
            tmp.data(), tmp.size(), compBuffer.get());
        w.WriteAs<uint64_t>(pathIndexesSize);
        w.WriteContiguous(compBuffer.get(), pathIndexesSize);
        
        // fieldSetIndexes.
        std::transform(_specs.begin(), _specs.end(), tmp.begin(),
                       [](Spec const &s) { return s.fieldSetIndex.value; });
        size_t fsetIndexesSize = Sdf_IntegerCompression::CompressToBuffer(
            tmp.data(), tmp.size(), compBuffer.get());
        w.WriteAs<uint64_t>(fsetIndexesSize);
        w.WriteContiguous(compBuffer.get(), fsetIndexesSize);
        
        // specTypes.
        std::transform(_specs.begin(), _specs.end(), tmp.begin(),
                       [](Spec const &s) { return s.specType; });
        size_t specTypesSize = Sdf_IntegerCompression::CompressToBuffer(
            tmp.data(), tmp.size(), compBuffer.get());
        w.WriteAs<uint64_t>(specTypesSize);
        w.WriteContiguous(compBuffer.get(), specTypesSize);
    }
}

template <class Iter>
Iter
CrateFile::_WritePathTree(_Writer &w, Iter cur, Iter end)
{
    // Each element looks like this:
    //
    // (pathIndex, pathElementTokenIndex, hasChild, hasSibling)
    // [offset to sibling, if hasSibling and hasChild]
    //
    // If the element's hasChild bit is set, then the very next element is its
    // first child.  If the element's hasChild bit is not set and its hasSibling
    // bit is set, then the very next element is its next sibling.  If both bits
    // are set then an offset to the sibling appears in the stream and the
    // following element is the first child.
    //

    for (Iter next = cur; cur != end; cur = next) {
        Iter nextSubtree = cur.GetNextSubtree();
        ++next;

        bool hasChild = next != nextSubtree &&
            next->first.GetParentPath() == cur->first;

        bool hasSibling = nextSubtree != end &&
            nextSubtree->first.GetParentPath() == cur->first.GetParentPath();

        bool isPrimPropertyPath = cur->first.IsPrimPropertyPath();

        auto elementToken = isPrimPropertyPath ?
            cur->first.GetNameToken() : cur->first.GetElementToken();

        // VERSIONING: If we're writing version 0.0.1, make sure we use the
        // right header type.
        if (_packCtx->writeVersion == Version(0,0,1)) {
            _PathItemHeader_0_0_1 header(
                cur->second, _GetIndexForToken(elementToken),
                static_cast<uint8_t>(
                    (hasChild ? _PathItemHeader::HasChildBit : 0) |
                    (hasSibling ? _PathItemHeader::HasSiblingBit : 0) |
                    (isPrimPropertyPath ?
                     _PathItemHeader::IsPrimPropertyPathBit : 0)));
            w.Write(header);
        } else {
            _PathItemHeader header(
                cur->second, _GetIndexForToken(elementToken),
                static_cast<uint8_t>(
                    (hasChild ? _PathItemHeader::HasChildBit : 0) |
                    (hasSibling ? _PathItemHeader::HasSiblingBit : 0) |
                    (isPrimPropertyPath ?
                     _PathItemHeader::IsPrimPropertyPathBit : 0)));
            w.Write(header);
        }

        // If there's both a child and a sibling, make space for the sibling
        // offset.
        int64_t siblingPtrOffset = -1;
        if (hasSibling && hasChild) {
            siblingPtrOffset = w.Tell();
            // Temporarily write a bogus value just to make space.
            w.WriteAs<int64_t>(-1);
        }
        // If there is a child, recurse.
        if (hasChild)
            next = _WritePathTree(w, next, end);

        // If we have a sibling, then fill in the offset that it will be
        // written at (it will be written next).
        if (hasSibling && hasChild) {
            int64_t cur = w.Tell();
            w.Seek(siblingPtrOffset);
            w.Write(cur);
            w.Seek(cur);
        }

        if (!hasSibling)
            return next;
    }
    return end;
}

template <class Iter>
Iter
CrateFile::_BuildCompressedPathDataRecursive(
    size_t &curIndex, Iter cur, Iter end,
    vector<uint32_t> &pathIndexes,
    vector<int32_t> &elementTokenIndexes,
    vector<int32_t> &jumps)
{
    auto getNextSubtree = [](Iter cur, Iter end) {
        Iter start = cur;
        while (cur != end && cur->first.HasPrefix(start->first)) {
            ++cur;
        }
        return cur;
    };
    
    for (Iter next = cur; cur != end; cur = next) {

        Iter nextSubtree = getNextSubtree(cur, end);
        ++next;

        bool hasChild = next != nextSubtree &&
            next->first.GetParentPath() == cur->first;

        bool hasSibling = nextSubtree != end &&
            nextSubtree->first.GetParentPath() == cur->first.GetParentPath();

        bool isPrimPropertyPath = cur->first.IsPrimPropertyPath();

        auto elementToken = isPrimPropertyPath ?
            cur->first.GetNameToken() : cur->first.GetElementToken();

        size_t thisIndex = curIndex++;
        pathIndexes[thisIndex] = cur->second.value;
        elementTokenIndexes[thisIndex] = _GetIndexForToken(elementToken).value;
        if (isPrimPropertyPath) {
            elementTokenIndexes[thisIndex] = -elementTokenIndexes[thisIndex];
        }

        // If there is a child, recurse.
        if (hasChild) {
            next = _BuildCompressedPathDataRecursive(
                curIndex, next, end, pathIndexes, elementTokenIndexes, jumps);
        }

        // If we have a sibling, then fill in the offset that it will be
        // written at (it will be written next).
        if (hasSibling && hasChild) {
            jumps[thisIndex] = curIndex-thisIndex;
        } else if (hasSibling) {
            jumps[thisIndex] = 0;
        } else if (hasChild) {
            jumps[thisIndex] = -1;
        } else {
            jumps[thisIndex] = -2;
        }

        if (!hasSibling)
            return next;
    }
    return end;
}

template <class Container>
void
CrateFile::_WriteCompressedPathData(_Writer &w, Container const &pathVec)
{
    // We build up three integer arrays representing the paths:
    // - pathIndexes[] :
    //     the index in _paths corresponding to this item.
    // - elementTokenIndexes[] :
    //     the element to append to the parent to get this path -- negative
    //     elements are prim property path elements.
    // - jumps[] :
    //     0=only a sibling, -1=only a child, -2=leaf, else has both, positive
    //     sibling index offset.
    //
    // This is vaguely similar to the _PathItemHeader struct used in prior
    // versions.

    // Write the # of encoded paths.  This can differ from the size of _paths
    // since we do not write out the empty path.
    w.WriteAs<uint64_t>(pathVec.size());
    
    vector<uint32_t> pathIndexes;
    vector<int32_t> elementTokenIndexes;
    vector<int32_t> jumps;

    pathIndexes.resize(pathVec.size());
    elementTokenIndexes.resize(pathVec.size());
    jumps.resize(pathVec.size());

    size_t index = 0;
    _BuildCompressedPathDataRecursive(
        index, pathVec.begin(), pathVec.end(),
        pathIndexes, elementTokenIndexes, jumps);

    // Compress and store the arrays.
    std::unique_ptr<char[]> compBuffer(
        new char[Sdf_IntegerCompression::
                 GetCompressedBufferSize(pathVec.size())]);

    // pathIndexes.
    uint64_t pathIndexesSize = Sdf_IntegerCompression::CompressToBuffer(
        pathIndexes.data(), pathIndexes.size(), compBuffer.get());
    w.WriteAs<uint64_t>(pathIndexesSize);
    w.WriteContiguous(compBuffer.get(), pathIndexesSize);

    // elementTokenIndexes.
    uint64_t elemToksSize = Sdf_IntegerCompression::CompressToBuffer(
        elementTokenIndexes.data(), elementTokenIndexes.size(),
        compBuffer.get());
    w.WriteAs<uint64_t>(elemToksSize);
    w.WriteContiguous(compBuffer.get(), elemToksSize);

    // jumps.
    uint64_t jumpsSize = Sdf_IntegerCompression::CompressToBuffer(
        jumps.data(), jumps.size(), compBuffer.get());
    w.WriteAs<uint64_t>(jumpsSize);
    w.WriteContiguous(compBuffer.get(), jumpsSize);
}

void
CrateFile::_WriteTokens(_Writer &w) {
    // # of strings.
    w.WriteAs<uint64_t>(_tokens.size());
    if (_packCtx->writeVersion < Version(0,4,0)) {
        // Count total bytes.
        uint64_t totalBytes = 0;
        for (auto const &t: _tokens)
            totalBytes += t.GetString().size() + 1;
        w.WriteAs<uint64_t>(totalBytes);
        // Token data.
        for (auto const &t: _tokens) {
            auto const &str = t.GetString();
            w.WriteContiguous(str.c_str(), str.size() + 1);
        }
    } else {
        // Version 0.4.0 compresses tokens.
        vector<char> tokenData;
        for (auto const &t: _tokens) {
            auto const &str = t.GetString();
            char const *cstr = str.c_str();
            tokenData.insert(tokenData.end(), cstr, cstr + str.size() + 1);
        }
        w.WriteAs<uint64_t>(tokenData.size());
        std::unique_ptr<char[]> compressed(
            new char[TfFastCompression::GetCompressedBufferSize(tokenData.size())]);
        uint64_t compressedSize = TfFastCompression::CompressToBuffer(
            tokenData.data(), compressed.get(), tokenData.size());
        w.WriteAs<uint64_t>(compressedSize);
        w.WriteContiguous(compressed.get(), compressedSize);
    }
}

template <class Reader>
void
CrateFile::_ReadStructuralSections(Reader reader, int64_t fileSize)
{
    TfErrorMark m;
    try{
        _boot = _ReadBootStrap(reader.src, fileSize);
        if (m.IsClean()) _toc = _ReadTOC(reader, _boot);
        if (m.IsClean()) _PrefetchStructuralSections(reader);
        if (m.IsClean()) _ReadTokens(reader);
        if (m.IsClean()) _ReadStrings(reader);
        if (m.IsClean()) _ReadFields(reader);
        if (m.IsClean()) _ReadFieldSets(reader);
        if (m.IsClean()) _ReadPaths(reader);
        if (m.IsClean()) _ReadSpecs(reader);
    } catch (const std::exception &e){
        TF_RUNTIME_ERROR("Encountered: %s, while reading @%s@", 
            e.what(), _assetPath.c_str());
        _specs.clear();
        _fieldSets.clear();
        _fields.clear();
    }

    if constexpr (SafetyOverSpeed) {
        if (m.IsClean()) {
            auto errorAndClear = [this]() {
                TF_RUNTIME_ERROR("Corrupt asset @%s@", _assetPath.c_str());
                _specs.clear();
                _fieldSets.clear();
                _fields.clear();
            };

            // Sanity check structural validity.

            // Fields.
            for (Field const &f: _fields) {
                if (f.tokenIndex.value >= _tokens.size()) {
                    errorAndClear();
                    return;
                }
            }

            // FieldSets.
            for (FieldIndex fi: _fieldSets) {
                // Default-constructed FieldIndex terminates field runs,
                // otherwise must index into _fields.
                if (fi != FieldIndex() && fi.value >= _fields.size()) {
                    errorAndClear();
                    return;
                }
            }

            // Specs.
            for (Spec const &spec: _specs) {
                // Range check path indexes, fieldSet indexes, spec types.
                // Additionally, a fieldSetIndex must either be 0, or the
                // element at the prior index must be a default-constructed
                // FieldIndex.
                if (spec.pathIndex.value >= _paths.size() ||
                    spec.fieldSetIndex.value >= _fieldSets.size() ||
                    (spec.fieldSetIndex.value &&
                     _fieldSets[spec.fieldSetIndex.value-1] != FieldIndex()) ||
                    spec.specType == SdfSpecTypeUnknown ||
                    spec.specType >= SdfNumSpecTypes) {
                    errorAndClear();
                    return;
                }
            }
        }
    } // SafetyOverSpeed
}

template <class ByteStream>
/*static*/
CrateFile::_BootStrap
CrateFile::_ReadBootStrap(ByteStream src, int64_t fileSize)
{
    _BootStrap b;
    if (fileSize < (int64_t)sizeof(_BootStrap)) {
        TF_RUNTIME_ERROR("File too small to contain bootstrap structure");
        return b;
    }
    src.Seek(0);
    src.Read(&b, sizeof(b));
    // Sanity check.
    if (memcmp(b.ident, USDC_IDENT, sizeof(b.ident))) {
        TF_RUNTIME_ERROR("Sdf crate bootstrap section corrupt");
    }
    // Check version.
    else if (!_SoftwareVersion.CanRead(Version(b))) {
        TF_RUNTIME_ERROR(
            "Sdf crate file version mismatch -- file is %s, "
            "software supports %s", Version(b).AsString().c_str(),
            _SoftwareVersion.AsString().c_str());
    }
    // Check that the table of contents is not past the end of the file.  This
    // catches some cases where a file was corrupted by truncation.
    else if (fileSize <= b.tocOffset) {
        TF_RUNTIME_ERROR(
            "Sdf crate file corrupt, possibly truncated: table of contents "
            "at offset %" PRId64 " but file size is %" PRId64,
            b.tocOffset, fileSize);
    }
    return b;
}

template <class Reader>
void
CrateFile::_PrefetchStructuralSections(Reader reader) const
{
    // Go through the _toc and find its maximal range, then ask the reader to
    // prefetch that range.
    int64_t min = -1, max = -1;
    for (_Section const &sec: _toc.sections) {
        if (min == -1 || (sec.start < min))
            min = sec.start;
        int64_t end = sec.start + sec.size;
        if (max == -1 || (end > max))
            max = end;
    }
    if (min != -1 && max != -1)
        reader.Prefetch(min, max-min);
}

template <class Reader>
CrateFile::_TableOfContents
CrateFile::_ReadTOC(Reader reader, _BootStrap const &b) const
{
    reader.Seek(b.tocOffset);
    return reader.template Read<_TableOfContents>();
}

template <class Reader>
void
CrateFile::_ReadFieldSets(Reader reader)
{
    TfAutoMallocTag tag("_ReadFieldSets");
    if (auto fieldSetsSection = _toc.GetSection(_FieldSetsSectionName)) {
        reader.Seek(fieldSetsSection->start);

        if (Version(_boot) < Version(0,4,0)) {
            _fieldSets = reader.template Read<decltype(_fieldSets)>();
        } else {
            // Compressed fieldSets in 0.4.0.
            auto numFieldSets = reader.template Read<uint64_t>();
            _fieldSets.resize(numFieldSets);
            vector<uint32_t> tmp(numFieldSets);
            _ReadCompressedInts(reader, tmp.data(), numFieldSets);
            for (size_t i = 0; i != numFieldSets; ++i) {
                _fieldSets[i].value = tmp[i];
            }
        }

        // FieldSets must be terminated by a default-constructed FieldIndex.
        if (!_fieldSets.empty() && _fieldSets.back() != FieldIndex()) {
            TF_RUNTIME_ERROR("Corrupt field sets in crate file");
            _fieldSets.back() = FieldIndex();
        }
    }
}

template <class Reader>
void
CrateFile::_ReadFields(Reader reader)
{
    TfAutoMallocTag tag("_ReadFields");
    if (auto fieldsSection = _toc.GetSection(_FieldsSectionName)) {
        reader.Seek(fieldsSection->start);
        if (Version(_boot) < Version(0,4,0)) {
            _fields = reader.template Read<decltype(_fields)>();
        } else {
            // Compressed fields in 0.4.0.
            auto numFields = reader.template Read<uint64_t>();
            _fields.resize(numFields);
            vector<uint32_t> tmp(numFields);
            _ReadCompressedInts(reader, tmp.data(), tmp.size());
            for (size_t i = 0; i != numFields; ++i) {
                _fields[i].tokenIndex.value = tmp[i];
            }

            // Compressed value reps.
            uint64_t repsSize = reader.template Read<uint64_t>();
            std::unique_ptr<char[]> compBuffer(new char[repsSize]);
            reader.ReadContiguous(compBuffer.get(), repsSize);
            vector<uint64_t> repsData;
            repsData.resize(numFields);
            TfFastCompression::DecompressFromBuffer(
                compBuffer.get(), reinterpret_cast<char *>(repsData.data()),
                repsSize, repsData.size() * sizeof(repsData[0]));

            for (size_t i = 0; i != numFields; ++i) {
                _fields[i].valueRep.data = repsData[i];
            }            
        }
    }
}

template <class Reader>
void
CrateFile::_ReadSpecs(Reader reader)
{
    TfAutoMallocTag tag("_ReadSpecs");
    if (auto specsSection = _toc.GetSection(_SpecsSectionName)) {
        reader.Seek(specsSection->start);
        // VERSIONING: Have to read either old or new style specs.
        if (Version(_boot) == Version(0,0,1)) {
            vector<Spec_0_0_1> old = reader.template Read<decltype(old)>();
            _specs.resize(old.size());
            copy(old.begin(), old.end(), _specs.begin());
        } else if (Version(_boot) < Version(0,4,0)) {
            _specs = reader.template Read<decltype(_specs)>();
        } else {
            // Version 0.4.0 specs are compressed
            auto numSpecs = reader.template Read<uint64_t>();
            _specs.resize(numSpecs);

            // Create temporary space for decompressing.
            _CompressedIntsReader cr;
            vector<uint32_t> tmp(numSpecs);

            // pathIndexes.
            cr.Read(reader, tmp.data(), numSpecs);
            for (size_t i = 0; i != numSpecs; ++i) {
                _specs[i].pathIndex.value = tmp[i];
            }

            // fieldSetIndexes.
            cr.Read(reader, tmp.data(), numSpecs);
            for (size_t i = 0; i != numSpecs; ++i) {
                _specs[i].fieldSetIndex.value = tmp[i];
            }
            
            // specTypes.
            cr.Read(reader, tmp.data(), numSpecs);
            for (size_t i = 0; i != numSpecs; ++i) {
                _specs[i].specType = static_cast<SdfSpecType>(tmp[i]);
            }
        }
    }

    if constexpr (SafetyOverSpeed) {
        // Spec sanity checks, in "prefer-safety-over-speed" mode.
        pxr_tsl::robin_set<SdfPath, SdfPath::Hash> seenPaths;

        std::vector<std::string> messages;

        for (Spec &spec: _specs) {
            // Check for valid-looking specs (no empty paths, no repeated paths,
            // valid SdfSpecType enum values...)
            SdfPath const &specPath = GetPath(spec.pathIndex);
            if (specPath.IsEmpty()) {
                messages.push_back(
                    TfStringPrintf("spec at index %zu has empty path",
                                   std::distance(&_specs.front(), &spec))); 
                // Mark for removal. 
                spec.specType = SdfSpecTypeUnknown;
                continue;
            }
            if (spec.specType == SdfSpecTypeUnknown ||
                spec.specType >= SdfNumSpecTypes) {
                messages.push_back(
                    TfStringPrintf("spec <%s> has %s",
                                   specPath.GetAsString().c_str(),
                                   spec.specType == SdfSpecTypeUnknown ?
                                   "unknown spec type" :
                                   TfStringPrintf("invalid spec type value %d",
                                                  spec.specType).c_str()));
                // Mark for removal.
                spec.specType = SdfSpecTypeUnknown;
                continue;
            }
            if (!seenPaths.insert(specPath).second) {
                messages.push_back(
                    TfStringPrintf("spec <%s> repeated",
                                   specPath.GetAsString().c_str()));
                // Mark for removal.
                spec.specType = SdfSpecTypeUnknown;
                continue;
            }
        }

        if (!messages.empty()) {
            // Remove everything with specType == Unknown -- any failed tests
            // above set specs that failed to have this spec type.
            _specs.erase(
                std::remove_if(_specs.begin(), _specs.end(),
                               [](Spec const &s) {
                                   return s.specType == SdfSpecTypeUnknown;
                               }),
                _specs.end());

            // Sort and unique the messages, then emit a warning.
            std::sort(messages.begin(), messages.end(), TfDictionaryLessThan());
            messages.erase(std::unique(messages.begin(), messages.end()),
                           messages.end());
            TF_RUNTIME_ERROR(
                "Corrupt asset @%s@ - ignoring invalid specs: %s.",
                _assetPath.c_str(), TfStringJoin(messages, ", ").c_str());
        }
    } // SafetyOverSpeed
}

template <class Reader>
void
CrateFile::_ReadStrings(Reader reader)
{
    TfAutoMallocTag tag("_ReadStrings");
    if (auto stringsSection = _toc.GetSection(_StringsSectionName)) {
        reader.Seek(stringsSection->start);
        _strings = reader.template Read<decltype(_strings)>();
    }
}

template <class Reader>
void
CrateFile::_ReadTokens(Reader reader)
{
    TfAutoMallocTag tag("_ReadTokens");

    auto tokensSection = _toc.GetSection(_TokensSectionName);
    if (!tokensSection)
        return;

    reader.Seek(tokensSection->start);

    // Read number of tokens.
    auto numTokens = reader.template Read<uint64_t>();

    RawDataPtr chars;
    char const *charsEnd = nullptr;
    
    Version fileVer(_boot);
    if (fileVer < Version(0,4,0)) {
        // XXX: To support pread(), we need to read the whole thing into memory
        // to make tokens out of it.  This is a pessimization vs mmap, from
        // which we can just construct from the chars directly.
        auto tokensNumBytes = reader.template Read<uint64_t>();
        chars.reset(new char[tokensNumBytes]);
        charsEnd = chars.get() + tokensNumBytes;
        reader.ReadContiguous(chars.get(), tokensNumBytes);
    } else {
        // Compressed token data.
        uint64_t uncompressedSize = reader.template Read<uint64_t>();
        uint64_t compressedSize = reader.template Read<uint64_t>();
        chars.reset(new char[uncompressedSize]);
        charsEnd = chars.get() + uncompressedSize;
        RawDataPtr compressed(new char[compressedSize]);
        reader.ReadContiguous(compressed.get(), compressedSize);
        TfFastCompression::DecompressFromBuffer(
            compressed.get(), chars.get(), compressedSize, uncompressedSize);
    }

    // Check/ensure that we're null terminated.
    if (chars.get() != charsEnd && charsEnd[-1] != '\0') {
        TF_RUNTIME_ERROR("Tokens section not null-terminated in crate file");
        const_cast<char *>(charsEnd)[-1] = '\0';
    }

    // Now we read that many null-terminated strings into _tokens.
    char const *p = chars.get();
    _tokens.clear();
    _tokens.resize(numTokens);

    WorkDispatcher wd;
    struct MakeToken {
        void operator()() const { (*tokens)[index] = TfToken(str); }
        vector<TfToken> *tokens;
        size_t index;
        char const *str;
    };
    size_t i = 0;
    for (; p < charsEnd && i != numTokens; ++i) {
        MakeToken mt { &_tokens, i, p };
        wd.Run(mt);
        p += strlen(p) + 1;
    }
    wd.Wait();
    if (i != numTokens) {
        TF_RUNTIME_ERROR("Crate file claims %zu tokens, found %zu",
                         numTokens, i);
    }

    WorkSwapDestroyAsync(chars);
}

template <class Reader>
void
CrateFile::_ReadPaths(Reader reader)
{
    TfAutoMallocTag tag("_ReadPaths");

    auto pathsSection = _toc.GetSection(_PathsSectionName);
    if (!pathsSection)
        return;

    reader.Seek(pathsSection->start);

    // Read # of paths, and fill the _paths vector with empty paths.
    _paths.resize(reader.template Read<uint64_t>());
    std::fill(_paths.begin(), _paths.end(), SdfPath());

    WorkDispatcher dispatcher;
    // VERSIONING: PathItemHeader changes size from 0.0.1 to 0.1.0.
    Version fileVer(_boot);
    if (fileVer == Version(0,0,1)) {
        _ReadPathsImpl<_PathItemHeader_0_0_1>(reader, dispatcher);
    } else if (fileVer < Version(0,4,0)) {
        _ReadPathsImpl<_PathItemHeader>(reader, dispatcher);
    } else {
        // 0.4.0 has compressed paths.
        _ReadCompressedPaths(reader, dispatcher);
    }
}

template <class Header, class Reader>
void
CrateFile::_ReadPathsImpl(Reader reader,
                          WorkDispatcher &dispatcher,
                          SdfPath parentPath)
{
    bool hasChild = false, hasSibling = false;
    do {
        auto h = reader.template Read<Header>();
        if (parentPath.IsEmpty()) {
            parentPath = SdfPath::AbsoluteRootPath();
            _paths[h.index.value] = parentPath;
        } else {
            auto const &elemToken = _tokens[h.elementTokenIndex.value];
            _paths[h.index.value] =
                h.bits & _PathItemHeader::IsPrimPropertyPathBit ?
                parentPath.AppendProperty(elemToken) :
                parentPath.AppendElementToken(elemToken);
        }

        // If we have either a child or a sibling but not both, then just
        // continue to the neighbor.  If we have both then spawn a task for the
        // sibling and do the child ourself.  We think that our path trees tend
        // to be broader more often than deep.

        hasChild = h.bits & _PathItemHeader::HasChildBit;
        hasSibling = h.bits & _PathItemHeader::HasSiblingBit;

        if (hasChild) {
            if (hasSibling) {
                // Branch off a parallel task for the sibling subtree.
                auto siblingOffset = reader.template Read<int64_t>();
                dispatcher.Run(
                    [this, reader,
                     siblingOffset, &dispatcher, parentPath]() {
                        // XXX Remove these tags when bug #132031 is addressed
                        TfAutoMallocTag tag(
                            "Sdf", "Sdf_CrateDataImpl::Open",
                            "Sdf_CrateFile::CrateFile::Open", "_ReadPaths");
                        auto readerCopy = reader;
                        readerCopy.Seek(siblingOffset);
                        _ReadPathsImpl<Header>(readerCopy, dispatcher, parentPath);
                    });
            }
            // Have a child (may have also had a sibling). Reset parent path.
            parentPath = _paths[h.index.value];
        }
        // If we had only a sibling, we just continue since the parent path is
        // unchanged and the next thing in the reader stream is the sibling's
        // header.
    } while (hasChild || hasSibling);
}

template <class Reader>
void
CrateFile::_ReadCompressedPaths(Reader reader,
                                WorkDispatcher &dispatcher)
{
    // Read compressed data first.
    vector<uint32_t> pathIndexes;
    vector<int32_t> elementTokenIndexes;
    vector<int32_t> jumps;

    // Read number of encoded paths.
    size_t numPaths = reader.template Read<uint64_t>();
    
    _CompressedIntsReader cr;

    // pathIndexes.
    pathIndexes.resize(numPaths);
    cr.Read(reader, pathIndexes.data(), numPaths);

    // Range check the pathIndexes, which index into _paths, and also ensure
    // there are no duplicates in pathIndexes.  If there are this file is
    // corrupt, and if we were to continue we could data-race while mutating the
    // same SdfPath object in _paths concurrently in
    // _BuildDecompressedPathsImpl.  It's a delightful occasion to use C++'s
    // collectio non grata: vector<bool>.
    if constexpr (SafetyOverSpeed) {
        vector<bool> seenIndexes(_paths.size());
        for (const uint32_t pathIndex: pathIndexes) {
            if (pathIndex >= _paths.size() || seenIndexes[pathIndex]) {
                TF_RUNTIME_ERROR(
                    "Corrupt path index in crate file (%u %s)",
                    pathIndex,
                    pathIndex >= _paths.size()
                    ? TfStringPrintf(">= %zu", _paths.size()).c_str()
                    : "repeated");
                return;
            }
            seenIndexes[pathIndex] = true;
        }
    } // SafetyOverSpeed
    
    // elementTokenIndexes.
    elementTokenIndexes.resize(numPaths);
    cr.Read(reader, elementTokenIndexes.data(), numPaths);

    // Range check the elementTokenIndexes, which index (by absolute value) into
    // _tokens.
    if constexpr (SafetyOverSpeed) {
        for (const int32_t elementTokenIndex: elementTokenIndexes) {
            if (static_cast<size_t>(
                    std::abs(elementTokenIndex)) >= _tokens.size()) {
                TF_RUNTIME_ERROR("Corrupt path element token index in crate "
                                 "file (%d >= %zu)",
                                 std::abs(elementTokenIndex), _tokens.size());
                return;
            }
        }
    } // SafetyOverSpeed

    // jumps.
    jumps.resize(numPaths);
    cr.Read(reader, jumps.data(), numPaths);

    // Now build the paths.
    _BuildDecompressedPathsImpl(pathIndexes, elementTokenIndexes, jumps, 0,
                                SdfPath(), dispatcher);

    dispatcher.Wait();
}

void
CrateFile::_BuildDecompressedPathsImpl(
    vector<uint32_t> const &pathIndexes,
    vector<int32_t> const &elementTokenIndexes,
    vector<int32_t> const &jumps,
    size_t curIndex,
    SdfPath parentPath,
    WorkDispatcher &dispatcher)
{
    bool hasChild = false, hasSibling = false;
    do {
        auto thisIndex = curIndex++;
        
        if constexpr (SafetyOverSpeed) {
            // Range check thisIndex.
            if (thisIndex >= pathIndexes.size()) {
                TF_RUNTIME_ERROR("Corrupt paths encoding in crate file "
                                 "(index:%zu >= %zu)",
                                 thisIndex, pathIndexes.size());
                return;
            }
        } // SafetyOverSpeed
        
        if (parentPath.IsEmpty()) {
            parentPath = SdfPath::AbsoluteRootPath();
            _paths[pathIndexes[thisIndex]] = parentPath;
        } else {
            int32_t tokenIndex = elementTokenIndexes[thisIndex];
            bool isPrimPropertyPath = tokenIndex < 0;
            tokenIndex = std::abs(tokenIndex);
            auto const &elemToken = _tokens[tokenIndex];
            _paths[pathIndexes[thisIndex]] =
                isPrimPropertyPath ?
                parentPath.AppendProperty(elemToken) :
                parentPath.AppendElementToken(elemToken);
        }

        // If we have either a child or a sibling but not both, then just
        // continue to the neighbor.  If we have both then spawn a task for the
        // sibling and do the child ourself.  We think that our path trees tend
        // to be broader more often than deep.

        hasChild = (jumps[thisIndex] > 0) || (jumps[thisIndex] == -1);
        hasSibling = (jumps[thisIndex] >= 0);

        if (hasChild) {
            if (hasSibling) {
                // Branch off a parallel task for the sibling subtree.
                auto siblingIndex = thisIndex + jumps[thisIndex];

                if constexpr (SafetyOverSpeed) {
                    // Range check siblingIndex, which indexes into pathIndexes.
                    if (siblingIndex >= pathIndexes.size()) {
                        TF_RUNTIME_ERROR(
                            "Corrupt paths jumps table in crate file "
                            "(jump:%d + thisIndex:%zu >= %zu)",
                            jumps[thisIndex], thisIndex, pathIndexes.size());
                        return;
                    }
                } // SafetyOverSpeed

                dispatcher.Run(
                    [this, &pathIndexes, &elementTokenIndexes, &jumps,
                     siblingIndex, &dispatcher, parentPath]()  {
                        // XXX Remove these tags when bug #132031 is addressed
                        TfAutoMallocTag tag(
                            "Sdf", "Sdf_CrateDataImpl::Open",
                            "Sdf_CrateFile::CrateFile::Open", "_ReadPaths");
                        _BuildDecompressedPathsImpl(
                            pathIndexes, elementTokenIndexes, jumps,
                            siblingIndex, parentPath, dispatcher);
                    });
            }
            // Have a child (may have also had a sibling). Reset parent path.
            parentPath = _paths[pathIndexes[thisIndex]];
        }
        // If we had only a sibling, we just continue since the parent path is
        // unchanged and the next thing in the reader stream is the sibling's
        // header.
    } while (hasChild || hasSibling);
}

void
CrateFile::_ReadRawBytes(int64_t start, int64_t size, char *buf) const
{
    if (_useMmap) {
        auto reader =
            _MakeReader(_MakeMmapStream(&_mmapSrc, _debugPageMap.get()));
        reader.Seek(start);
        reader.template ReadContiguous<char>(buf, size);
    } else if (_preadSrc) {
        auto reader = _MakeReader(_PreadStream(_preadSrc));
        reader.Seek(start);
        reader.template ReadContiguous<char>(buf, size);
    } else {
        auto reader = _MakeReader(_AssetStream(_assetSrc));
        reader.Seek(start);
        reader.template ReadContiguous<char>(buf, size);
    }
}

PathIndex
CrateFile::_AddPath(const SdfPath &path)
{
    // Try to insert this path.
    auto iresult = _packCtx->pathToPathIndex.emplace(path, PathIndex());
    if (iresult.second) {
        // If this is a target path, add the target.
        if (path.IsTargetPath())
            _AddPath(path.GetTargetPath());

        // Not present -- ensure parent is added.
        if (path != SdfPath::AbsoluteRootPath())
            _AddPath(path.GetParentPath());

        // Add a token for this path's element string, unless it's a prim
        // property path, in which case we add the name.  We treat prim property
        // paths separately since there are so many, and the name with the dot
        // just basically doubles the number of tokens we store.
        _AddToken(path.IsPrimPropertyPath() ? path.GetNameToken() :
                  path.GetElementToken());

        // Add to the vector and insert the index.
        iresult.first->second = PathIndex(_paths.size());
        _paths.emplace_back(path);
    }
    return iresult.first->second;
}

FieldSetIndex
CrateFile::_AddFieldSet(const std::vector<FieldIndex> &fieldIndexes)
{
    auto iresult =
        _packCtx->fieldsToFieldSetIndex.emplace(fieldIndexes, FieldSetIndex());
    if (iresult.second) {
        // Not yet present.  Copy the fields to _fieldSets, terminate, and store
        // the start index.
        iresult.first->second = FieldSetIndex(_fieldSets.size());
        _fieldSets.insert(_fieldSets.end(),
                          fieldIndexes.begin(), fieldIndexes.end());
        _fieldSets.push_back(FieldIndex());
    }
    return iresult.first->second;
}

FieldIndex
CrateFile::_AddField(const FieldValuePair &fv)
{
    Field field(_AddToken(fv.first), _PackValue(fv.second));
    auto iresult = _packCtx->fieldToFieldIndex.emplace(field, FieldIndex());
    if (iresult.second) {
        // Not yet present.
        iresult.first->second = FieldIndex(_fields.size());
        _fields.push_back(field);
    }
    return iresult.first->second;
}

TokenIndex
CrateFile::_AddToken(const TfToken &token)
{
    auto iresult = _packCtx->tokenToTokenIndex.emplace(token, TokenIndex());
    if (iresult.second) {
        // Not yet present.
        iresult.first->second = TokenIndex(_tokens.size());
        _tokens.emplace_back(token);
    }
    return iresult.first->second;
}

TokenIndex
CrateFile::_GetIndexForToken(const TfToken &token) const
{
    auto iter = _packCtx->tokenToTokenIndex.find(token);
    if (!TF_VERIFY(iter != _packCtx->tokenToTokenIndex.end()))
        return TokenIndex();
    return iter->second;
}

StringIndex
CrateFile::_AddString(const string &str)
{
    auto iresult = _packCtx->stringToStringIndex.emplace(str, StringIndex());
    if (iresult.second) {
        // Not yet present.
        iresult.first->second = StringIndex(_strings.size());
        _strings.push_back(_AddToken(TfToken(str)));
    }
    return iresult.first->second;
}

template <class T>
CrateFile::_ValueHandler<T> &
CrateFile::_GetValueHandler() {
    return *static_cast<_ValueHandler<T> *>(
        _valueHandlers[static_cast<int>(_TypeEnumFor<T>::value)]);
}

template <class T>
CrateFile::_ValueHandler<T> const &
CrateFile::_GetValueHandler() const {
    return *static_cast<_ValueHandler<T> const *>(
        _valueHandlers[static_cast<int>(_TypeEnumFor<T>::value)]);
}

template <class T>
ValueRep
CrateFile::_PackValue(T const &v) {
    return _GetValueHandler<T>().Pack(_Writer(this), v);
}

template <class T>
ValueRep
CrateFile::_PackValue(VtArray<T> const &v) {
    return _GetValueHandler<T>().PackArray(_Writer(this), v);
}

ValueRep
CrateFile::_PackValue(VtValue const &v)
{
    // If the value is holding a ValueRep, then we can just return it, we don't
    // need to add anything.
    if (v.IsHolding<ValueRep>()) {
        const ValueRep &valueRep = v.UncheckedGet<ValueRep>();
        // Special case for packed SdfPayloads. If the packed value is from 
        // a pre 0.8.0 version but we're writing to a 0.8.0 or later file, we
        // need to unpack and repack the payload. Otherwise the packed payload
        // won't have a layer offset and will not be read correctly when reading
        // the new file.
        if (valueRep.GetType() == TypeEnum::Payload &&
            Version(_boot) < Version(0, 8, 0) && 
            _packCtx->writeVersion >= Version(0, 8, 0)) {
            VtValue payloadValue;
            _UnpackValue(valueRep, &payloadValue);
            return _PackValue(payloadValue);
        }
        return valueRep;
    }

    // Similarly if the value is holding a TimeSamples that is still reading
    // from the file, we can return its held rep and continue.
    if (v.IsHolding<TimeSamples>()) {
        auto const &ts = v.UncheckedGet<TimeSamples>();
        if (!ts.IsInMemory())
            return ts.valueRep;
    }

    std::type_index ti =
        v.IsArrayValued() ? v.GetElementTypeid() : v.GetTypeid();

    auto it = _packValueFunctions.find(ti);
    if (it != _packValueFunctions.end())
        return it->second(v);

    TF_CODING_ERROR("Attempted to pack unsupported type '%s' "
                    "(%s)", ArchGetDemangled(ti).c_str(),
                    TfStringify(v).c_str());

    return ValueRep(0);
}

template <class T>
void
CrateFile::_UnpackValue(ValueRep rep, T *out) const
{
    try {
        auto const &h = _GetValueHandler<T>();
        if (_useMmap) {
            h.Unpack(
                _MakeReader(
                    _MakeMmapStream(
                        &_mmapSrc, _debugPageMap.get())), rep, out);
        } else if (_preadSrc) {
            h.Unpack(_MakeReader(_PreadStream(_preadSrc)), rep, out);
        } else {
            h.Unpack(_MakeReader(_AssetStream(_assetSrc)), rep, out);
        }
    }
    catch (...) {
        TF_RUNTIME_ERROR("Corrupt asset <%s>: exception thrown unpacking a "
                         "%s, returning a value-initialized object",
                         GetAssetPath().c_str(),
                         ArchGetDemangled<T>().c_str());
        *out = T();
    }
}

template <class T>
void
CrateFile::_UnpackValue(ValueRep rep, VtArray<T> *out) const {
    try {
        auto const &h = _GetValueHandler<T>();
        if (_useMmap) {
            h.UnpackArray(_MakeReader(
                              _MakeMmapStream(
                                  &_mmapSrc, _debugPageMap.get())), rep, out);
        } else if (_preadSrc) {
            h.UnpackArray(_MakeReader(_PreadStream(_preadSrc)), rep, out);
        } else {
            h.UnpackArray(_MakeReader(_AssetStream(_assetSrc)), rep, out);
        }
    }
    catch (...) {
        TF_RUNTIME_ERROR("Corrupt asset <%s>: exception thrown unpacking a "
                         "VtArray<%s>, returning an empty array",
                         GetAssetPath().c_str(),
                         ArchGetDemangled<T>().c_str());
        *out = VtArray<T>();
    }
}

void
CrateFile::_UnpackValue(ValueRep rep, VtValue *result) const {
    // Look up the function for the type enum, and invoke it.
    auto repType = rep.GetType();
    if (repType == TypeEnum::Invalid || repType >= TypeEnum::NumTypes) {
        TF_CODING_ERROR("Attempted to unpack unsupported type enum value %d",
                        static_cast<int>(repType));
        return;
    }
    try {
        auto index = static_cast<int>(repType);
        if (_useMmap) {
            _unpackValueFunctionsMmap[index](rep, result);
        } else if (_preadSrc) {
            _unpackValueFunctionsPread[index](rep, result);
        } else {
            _unpackValueFunctionsAsset[index](rep, result);
        }
    }
    catch (...) {
        TF_RUNTIME_ERROR("Corrupt asset <%s>: exception thrown unpacking a "
                         "value, returning an empty VtValue",
                         GetAssetPath().c_str());
        *result = VtValue();
    }
}

std::type_info const &
CrateFile::GetTypeid(ValueRep rep) const
{
    switch (rep.GetType()) {
#define xx(ENUMNAME, _unused, T, SUPPORTSARRAY)                                \
        case TypeEnum::ENUMNAME:                                               \
            if constexpr (SUPPORTSARRAY) {                                     \
                return rep.IsArray() ? typeid(VtArray<T>) : typeid(T);         \
            }                                                                  \
            else {                                                             \
                return typeid(T);                                              \
            }

#include "crateDataTypes.h"

#undef xx
    default:
        return typeid(void);
    };
}

template <class T>
void
CrateFile::_DoTypeRegistration() {
    auto typeEnumIndex = static_cast<int>(_TypeEnumFor<T>::value);
    auto valueHandler = new _ValueHandler<T>();
    _valueHandlers[typeEnumIndex] = valueHandler;

    // Value Pack/Unpack functions.
    _packValueFunctions[std::type_index(typeid(T))] =
        [this, valueHandler](VtValue const &val) {
            return valueHandler->PackVtValue(_Writer(this), val);
        };

    _unpackValueFunctionsPread[typeEnumIndex] =
        [this, valueHandler](ValueRep rep, VtValue *out) {
            valueHandler->UnpackVtValue(
                _MakeReader(_PreadStream(_preadSrc)), rep, out);
        };

    _unpackValueFunctionsMmap[typeEnumIndex] =
        [this, valueHandler](ValueRep rep, VtValue *out) {
            valueHandler->UnpackVtValue(
                _MakeReader(_MakeMmapStream(
                                &_mmapSrc, _debugPageMap.get())), rep, out);
        };

    _unpackValueFunctionsAsset[typeEnumIndex] =
        [this, valueHandler](ValueRep rep, VtValue *out) {
            valueHandler->UnpackVtValue(
                _MakeReader(_AssetStream(_assetSrc)), rep, out);
        };
}

// Functions that populate the value read/write functions.
void
CrateFile::_DoAllTypeRegistrations() {
    TfAutoMallocTag tag("Sdf_CrateFile::CrateFile::_DoAllTypeRegistrations");
#define xx(_unused1, _unused2, CPPTYPE, _unused3)       \
    _DoTypeRegistration<CPPTYPE>();

#include "crateDataTypes.h"

#undef xx
}

void
CrateFile::_DeleteValueHandlers() {
#define xx(_unused1, _unused2, T, _unused3)                                    \
    delete static_cast<_ValueHandler<T> *>(                                    \
        _valueHandlers[static_cast<int>(_TypeEnumFor<T>::value)]);

#include "crateDataTypes.h"

#undef xx
}

void
CrateFile::_ClearValueHandlerDedupTables() {
#define xx(_unused1, _unused2, T, _unused3)                                    \
    static_cast<_ValueHandler<T> *>(                                           \
        _valueHandlers[static_cast<int>(_TypeEnumFor<T>::value)])->Clear();

#include "crateDataTypes.h"

#undef xx
}


/* static */
bool
CrateFile::_IsKnownSection(char const *name) {
    for (auto const &secName: _KnownSections) {
        if (secName == name)
            return true;
    }
    return false;
}

#ifdef PXR_PREFER_SAFETY_OVER_SPEED
CrateFile::Field const &
CrateFile::_GetEmptyField() const
{
    static Field empty;
    return empty;
}

std::string const &
CrateFile::_GetEmptyString() const
{
    static std::string empty;
    return empty;
}

TfToken const &
CrateFile::_GetEmptyToken() const
{
    static TfToken empty;
    return empty;
}
#endif // PXR_PREFER_SAFETY_OVER_SPEED

CrateFile::Spec::Spec(Spec_0_0_1 const &s) 
    : Spec(s.pathIndex, s.specType, s.fieldSetIndex) {}

CrateFile::Spec_0_0_1::Spec_0_0_1(Spec const &s) 
    : Spec_0_0_1(s.pathIndex, s.specType, s.fieldSetIndex) {}

CrateFile::_BootStrap::_BootStrap() : _BootStrap(_SoftwareVersion) {}

CrateFile::_BootStrap::_BootStrap(Version const &ver)
{
    memset(this, 0, sizeof(*this));
    tocOffset = 0;
    memcpy(ident, USDC_IDENT, sizeof(ident));
    version[0] = ver.majver;
    version[1] = ver.minver;
    version[2] = ver.patchver;
}

CrateFile::_Section::_Section(char const *inName, int64_t start, int64_t size)
    : start(start), size(size)
{
    memset(name, 0, sizeof(name));
    if (TF_VERIFY(strlen(inName) <= _SectionNameMaxLength))
        strcpy(name, inName);
}

std::ostream &
operator<<(std::ostream &o, ValueRep rep) {
    o << "ValueRep enum=" << int(rep.GetType());
    if (rep.IsArray())
        o << " (array)";
    return o << " payload=" << rep.GetPayload();
}

std::ostream &
operator<<(std::ostream &os, TimeSamples const &samples) {
    return os << "TimeSamples with " <<
        samples.times.Get().size() << " samples";
}

std::ostream &
operator<<(std::ostream &os, Index const &i) {
    return os << i.value;
}

// Size checks for structures written to/read from disk.
static_assert(sizeof(CrateFile::Field) == 16, "");
static_assert(sizeof(CrateFile::Spec) == 12, "");
static_assert(sizeof(CrateFile::Spec_0_0_1) == 16, "");
static_assert(sizeof(_PathItemHeader) == 12, "");
static_assert(sizeof(_PathItemHeader_0_0_1) == 16, "");

} // Sdf_CrateFile



PXR_NAMESPACE_CLOSE_SCOPE

