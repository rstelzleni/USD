
#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/valueDescriptor.h"

#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/base/vt/types.h"

#include "pxr/base/gf/matrix2f.h"
#include "pxr/base/gf/matrix2d.h"
#include "pxr/base/gf/matrix3d.h"
#include "pxr/base/gf/matrix3f.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/dualQuatf.h"
#include "pxr/base/gf/dualQuatd.h"
#include "pxr/base/gf/range2f.h"
#include "pxr/base/gf/range3d.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/token.h"

#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

static void testFloat() {
    VtValue v(1.234f);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "float");
    TF_AXIOM(desc.ToString() == "1.234");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(std::abs(desc.GetValue().Get<float>() - 1.234f) < 1e-10f);
}

static void testDouble() {
    VtValue v(1.234);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "double");
    TF_AXIOM(desc.ToString() == "1.234");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(std::abs(desc.GetValue().Get<double>() - 1.234) < 1e-10);
}

static void testHalf() {
    VtValue v(GfHalf(1.5));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "pxr_half::half");
    TF_AXIOM(desc.ToString() == "1.5");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(std::abs(float(desc.GetValue().Get<GfHalf>()) - 1.5f) < 1e-10f);
}

static void testInt() {
    VtValue v(123);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "int");
    TF_AXIOM(desc.ToString() == "123");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(desc.GetValue().Get<int>() == 123);
}

static void testBool() {
    VtValue v(true);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(desc.IsInteger()); // note, is integer and is bool are true
    TF_AXIOM(desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "bool");
    TF_AXIOM(desc.ToString() == "1"); // note, 1, not true (TfStringify output)
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(desc.GetValue().Get<bool>() == true);
}

static void testString() {
    VtValue v(std::string("hello"));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "string");
    TF_AXIOM(desc.ToString() == "hello");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(desc.GetValue().Get<std::string>() == "hello");
}

static void testToken() {
    VtValue v(TfToken("hello"));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "TfToken");
    TF_AXIOM(desc.ToString() == "hello");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    TF_AXIOM(desc.GetValue().Get<TfToken>() == TfToken("hello"));
}

static void testArrayInt() {
    VtArray<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<int>");
    TF_AXIOM(desc.ToString() == "[1, 2, 3]");
    TF_AXIOM(desc.GetArraySize() == 3);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    VtArray<int> arr2 = desc.GetValue().Get<VtArray<int>>();
    TF_AXIOM(arr2.size() == 3);
    TF_AXIOM(arr2[0] == 1);
    TF_AXIOM(arr2[1] == 2);
    TF_AXIOM(arr2[2] == 3);
}

static void testArrayFloat() {
    VtArray<float> arr;
    arr.push_back(1.0f);
    arr.push_back(2.0f);
    arr.push_back(3.0f);
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<float>");
    TF_AXIOM(desc.ToString() == "[1, 2, 3]");
    TF_AXIOM(desc.GetArraySize() == 3);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    VtArray<float> arr2 = desc.GetValue().Get<VtArray<float>>();
    TF_AXIOM(arr2.size() == 3);
    TF_AXIOM(std::abs(arr2[0] - 1.0f) < 1e-10f);
    TF_AXIOM(std::abs(arr2[1] - 2.0f) < 1e-10f);
    TF_AXIOM(std::abs(arr2[2] - 3.0f) < 1e-10f);
}

static void testArrayString() {
    VtArray<std::string> arr;
    arr.push_back("one");
    arr.push_back("two");
    arr.push_back("three");
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<string>");
    TF_AXIOM(desc.ToString() == "[one, two, three]");
    TF_AXIOM(desc.GetArraySize() == 3);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    VtArray<std::string> arr2 = desc.GetValue().Get<VtArray<std::string>>();
    TF_AXIOM(arr2.size() == 3);
    TF_AXIOM(arr2[0] == "one");
    TF_AXIOM(arr2[1] == "two");
    TF_AXIOM(arr2[2] == "three");
}

static void testGfVec2i() {
    VtValue v(GfVec2i(1, 2));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfVec2i");
    TF_AXIOM(desc.ToString() == "(1, 2)");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfVec2i v2 = desc.GetValue().Get<GfVec2i>();
    TF_AXIOM(v2[0] == 1);
    TF_AXIOM(v2[1] == 2);
}

static void testGfVec2h() {
    VtValue v(GfVec2h(1.0f, 2.0f));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfVec2h");
    TF_AXIOM(desc.ToString() == "(1, 2)");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfVec2h v2 = desc.GetValue().Get<GfVec2h>();
    TF_AXIOM(std::abs(float(v2[0]) - 1.0f) < 1e-10f);
    TF_AXIOM(std::abs(float(v2[1]) - 2.0f) < 1e-10f);
}

static void testGfVec4d() {
    VtValue v(GfVec4d(1.0, 2.0, 3.0, 4.0));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfVec4d");
    TF_AXIOM(desc.ToString() == "(1, 2, 3, 4)");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfVec4d v2 = desc.GetValue().Get<GfVec4d>();
    TF_AXIOM(std::abs(v2[0] - 1.0) < 1e-10);
    TF_AXIOM(std::abs(v2[1] - 2.0) < 1e-10);
    TF_AXIOM(std::abs(v2[2] - 3.0) < 1e-10);
    TF_AXIOM(std::abs(v2[3] - 4.0) < 1e-10);
}

static void testGfMatrix4f() {
    GfMatrix4f m(1.0f);
    m[3][3] = 2.0f;
    VtValue v(m);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfMatrix4f");
    TF_AXIOM(desc.ToString() == 
        "( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 2) )");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfMatrix4f m2 = desc.GetValue().Get<GfMatrix4f>();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i == j && i < 3) {
                TF_AXIOM(std::abs(m2[i][j] - 1.0f) < 1e-10f);
            } else if (i == j && i == 3) {
                TF_AXIOM(std::abs(m2[i][j] - 2.0f) < 1e-10f);
            } else {
                TF_AXIOM(std::abs(m2[i][j] - 0.0f) < 1e-10f);
            }
        }
    }
}

static void testGfMatrix3d() {
    GfMatrix3d m(1.0);
    m[2][2] = 2.0;
    VtValue v(m);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfMatrix3d");
    TF_AXIOM(desc.ToString() == 
        "( (1, 0, 0), (0, 1, 0), (0, 0, 2) )");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfMatrix3d m2 = desc.GetValue().Get<GfMatrix3d>();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i == j && i < 2) {
                TF_AXIOM(std::abs(m2[i][j] - 1.0) < 1e-10);
            } else if (i == j && i == 2) {
                TF_AXIOM(std::abs(m2[i][j] - 2.0) < 1e-10);
            } else {
                TF_AXIOM(std::abs(m2[i][j] - 0.0) < 1e-10);
            }
        }
    }
}

static void testGfMatrix2f() {
    GfMatrix2f m(1.0f);
    m[1][1] = 2.0f;
    VtValue v(m);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfMatrix2f");
    TF_AXIOM(desc.ToString() == 
        "( (1, 0), (0, 2) )");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfMatrix2f m2 = desc.GetValue().Get<GfMatrix2f>();
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (i == j && i == 0) {
                TF_AXIOM(std::abs(m2[i][j] - 1.0f) < 1e-10f);
            } else if (i == j && i == 1) {
                TF_AXIOM(std::abs(m2[i][j] - 2.0f) < 1e-10f);
            } else {
                TF_AXIOM(std::abs(m2[i][j] - 0.0f) < 1e-10f);
            }
        }
    }
}

static void testGfQuatd() {
    VtValue v(GfQuatd(1.0, GfVec3d(0.0, 1.0, 0.0)));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfQuatd");
    TF_AXIOM(desc.ToString() == "(1, 0, 1, 0)");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfQuatd q2 = desc.GetValue().Get<GfQuatd>();
    TF_AXIOM(std::abs(q2.GetReal() - 1.0) < 1e-10);
    GfVec3d v2 = q2.GetImaginary();
    TF_AXIOM(std::abs(v2[0] - 0.0) < 1e-10);
    TF_AXIOM(std::abs(v2[1] - 1.0) < 1e-10);
    TF_AXIOM(std::abs(v2[2] - 0.0) < 1e-10);
}

static void testGfDualQuatf() {
    VtValue v(GfDualQuatf(GfQuatf(1.0f, GfVec3f(0.0f, 1.0f, 0.0f)),
                          GfQuatf(0.0f, GfVec3f(1.0f, 0.0f, 0.0f))));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfDualQuatf");
    TF_AXIOM(desc.ToString() == "((1, 0, 1, 0), (0, 1, 0, 0))");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfDualQuatf dq2 = desc.GetValue().Get<GfDualQuatf>();
    GfQuatf qReal = dq2.GetReal();
    TF_AXIOM(std::abs(qReal.GetReal() - 1.0f) < 1e-10f);
    GfVec3f vReal = qReal.GetImaginary();
    TF_AXIOM(std::abs(vReal[0] - 0.0f) < 1e-10f);
    TF_AXIOM(std::abs(vReal[1] - 1.0f) < 1e-10f);
    TF_AXIOM(std::abs(vReal[2] - 0.0f) < 1e-10f);
    GfQuatf qDual = dq2.GetDual();
    TF_AXIOM(std::abs(qDual.GetReal() - 0.0f) < 1e-10f);
    GfVec3f vDual = qDual.GetImaginary();
    TF_AXIOM(std::abs(vDual[0] - 1.0f) < 1e-10f);
    TF_AXIOM(std::abs(vDual[1] - 0.0f) < 1e-10f);
    TF_AXIOM(std::abs(vDual[2] - 0.0f) < 1e-10f);
}

static void testGfRange3d() {
    VtValue v(GfRange3d(GfVec3d(1.0, 2.0, 3.0), GfVec3d(4.0, 5.0, 6.0)));
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(!desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "GfRange3d");
    TF_AXIOM(desc.ToString() == "[(1, 2, 3)...(4, 5, 6)]");
    TF_AXIOM(desc.GetArraySize() == 0);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 0);
    GfRange3d r2 = desc.GetValue().Get<GfRange3d>();
    GfVec3d min = r2.GetMin();
    GfVec3d max = r2.GetMax();
    TF_AXIOM(std::abs(min[0] - 1.0) < 1e-10);
    TF_AXIOM(std::abs(min[1] - 2.0) < 1e-10);
    TF_AXIOM(std::abs(min[2] - 3.0) < 1e-10);
    TF_AXIOM(std::abs(max[0] - 4.0) < 1e-10);
    TF_AXIOM(std::abs(max[1] - 5.0) < 1e-10);
    TF_AXIOM(std::abs(max[2] - 6.0) < 1e-10);
}

static void testArrayGfVec4i() {
    VtArray<GfVec4i> arr;
    arr.push_back(GfVec4i(1, 2, 3, 4));
    arr.push_back(GfVec4i(5, 6, 7, 8));
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(!desc.IsFloat());
    TF_AXIOM(desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfVec4i>");
    TF_AXIOM(desc.ToString() == "[(1, 2, 3, 4), (5, 6, 7, 8)]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 4);
    VtArray<GfVec4i> arr2 = desc.GetValue().Get<VtArray<GfVec4i>>();
    TF_AXIOM(arr2.size() == 2);
    TF_AXIOM(arr2[0] == GfVec4i(1, 2, 3, 4));
    TF_AXIOM(arr2[1] == GfVec4i(5, 6, 7, 8));
}

static void testArrayGfVec3f() {
    VtArray<GfVec3f> arr;
    arr.push_back(GfVec3f(1.0f, 2.0f, 3.0f));
    arr.push_back(GfVec3f(4.0f, 5.0f, 6.0f));
    arr.push_back(GfVec3f(7.0f, 8.0f, 9.0f));
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfVec3f>");
    TF_AXIOM(desc.ToString() == "[(1, 2, 3), (4, 5, 6), (7, 8, 9)]");
    TF_AXIOM(desc.GetArraySize() == 3);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 3);
    VtArray<GfVec3f> arr2 = desc.GetValue().Get<VtArray<GfVec3f>>();
    TF_AXIOM(arr2.size() == 3);
    TF_AXIOM(arr2[0] == GfVec3f(1.0f, 2.0f, 3.0f));
    TF_AXIOM(arr2[1] == GfVec3f(4.0f, 5.0f, 6.0f));
    TF_AXIOM(arr2[2] == GfVec3f(7.0f, 8.0f, 9.0f));
}

static void testArrayGfMatrix4d() {
    VtArray<GfMatrix4d> arr;
    GfMatrix4d m1(1.0);
    m1[3][3] = 2.0;
    GfMatrix4d m2(3.0);
    m2[3][3] = 4.0;
    arr.push_back(m1);
    arr.push_back(m2);
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfMatrix4d>");
    TF_AXIOM(desc.ToString() == 
        "[( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 2) ), "
        "( (3, 0, 0, 0), (0, 3, 0, 0), (0, 0, 3, 0), (0, 0, 0, 4) )]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 2);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 4);
    TF_AXIOM(desc.GetArrayItemDimension()[1] == 4);
    VtArray<GfMatrix4d> arr2 = desc.GetValue().Get<VtArray<GfMatrix4d>>();
    TF_AXIOM(arr2.size() == 2);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i == j && i < 3) {
                TF_AXIOM(std::abs(arr2[0][i][j] - 1.0) < 1e-10);
            } else if (i == j && i == 3) {
                TF_AXIOM(std::abs(arr2[0][i][j] - 2.0) < 1e-10);
            } else {
                TF_AXIOM(std::abs(arr2[0][i][j] - 0.0) < 1e-10);
            }
        }
    }
    for (int i = 0; i < 4; ++i) {        
        for (int j = 0; j < 4; ++j) {      
            if (i == j && i < 3) {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 3.0) < 1e-10);      
            } else if (i == j && i == 3) {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 4.0) < 1e-10);      
            } else {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 0.0) < 1e-10);      
            }      
        }
    }
}

static void testArrayGfMatrix3f() {
    VtArray<GfMatrix3f> arr;
    GfMatrix3f m1(1.0f);
    m1[2][2] = 2.0f;
    GfMatrix3f m2(3.0f);
    m2[2][2] = 4.0f;
    arr.push_back(m1);
    arr.push_back(m2);
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfMatrix3f>");
    TF_AXIOM(desc.ToString() == 
        "[( (1, 0, 0), (0, 1, 0), (0, 0, 2) ), "
        "( (3, 0, 0), (0, 3, 0), (0, 0, 4) )]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 2);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 3);
    TF_AXIOM(desc.GetArrayItemDimension()[1] == 3);
    VtArray<GfMatrix3f> arr2 = desc.GetValue().Get<VtArray<GfMatrix3f>>();
    TF_AXIOM(arr2.size() == 2);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i == j && i < 2) {
                TF_AXIOM(std::abs(arr2[0][i][j] - 1.0f) < 1e-10f);
            } else if (i == j && i == 2) {
                TF_AXIOM(std::abs(arr2[0][i][j] - 2.0f) < 1e-10f);
            } else {
                TF_AXIOM(std::abs(arr2[0][i][j] - 0.0f) < 1e-10f);
            }
        }
    }
    for (int i = 0; i < 3; ++i) {        
        for (int j = 0; j < 3; ++j) {   
            if (i == j && i < 2) {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 3.0f) < 1e-10f);      
            } else if (i == j && i == 2) {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 4.0f) < 1e-10f);      
            } else {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 0.0f) < 1e-10f);      
            }      
        }
    }
}

static void testArrayGfMatrix2d() {
    VtArray<GfMatrix2d> arr;
    GfMatrix2d m1(1.0);
    m1[1][1] = 2.0;
    GfMatrix2d m2(3.0);
    m2[1][1] = 4.0;
    arr.push_back(m1);
    arr.push_back(m2);
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfMatrix2d>");
    TF_AXIOM(desc.ToString() == 
        "[( (1, 0), (0, 2) ), "
        "( (3, 0), (0, 4) )]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 2);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 2);
    TF_AXIOM(desc.GetArrayItemDimension()[1] == 2);
    VtArray<GfMatrix2d> arr2 = desc.GetValue().Get<VtArray<GfMatrix2d>>();
    TF_AXIOM(arr2.size() == 2);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (i == j && i == 0) {
                TF_AXIOM(std::abs(arr2[0][i][j] - 1.0) < 1e-10);
            } else if (i == j && i == 1) {
                TF_AXIOM(std::abs(arr2[0][i][j] - 2.0) < 1e-10);
            } else {
                TF_AXIOM(std::abs(arr2[0][i][j] - 0.0) < 1e-10);
            }
        }
    }
    for (int i = 0; i < 2; ++i) {        
        for (int j = 0; j < 2; ++j) {   
            if (i == j && i == 0) {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 3.0) < 1e-10);      
            } else if (i == j && i == 1) {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 4.0) < 1e-10);      
            } else {      
                TF_AXIOM(std::abs(arr2[1][i][j] - 0.0) < 1e-10);      
            }      
        }
    }
}

static void testArrayGfQuatf() {
    VtArray<GfQuatf> arr;
    arr.push_back(GfQuatf(1.0f, GfVec3f(0.0f, 1.0f, 0.0f)));
    arr.push_back(GfQuatf(0.0f, GfVec3f(1.0f, 0.0f, 0.0f)));
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfQuatf>");
    TF_AXIOM(desc.ToString() == "[(1, 0, 1, 0), (0, 1, 0, 0)]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 4);
    VtArray<GfQuatf> arr2 = desc.GetValue().Get<VtArray<GfQuatf>>();
    TF_AXIOM(arr2.size() == 2);
    TF_AXIOM(arr2[0] == GfQuatf(1.0f, GfVec3f(0.0f, 1.0f, 0.0f)));
    TF_AXIOM(arr2[1] == GfQuatf(0.0f, GfVec3f(1.0f, 0.0f, 0.0f)));
}

static void testArrayGfDualQuatd() {
    VtArray<GfDualQuatd> arr;
    arr.push_back(GfDualQuatd(GfQuatd(1.0, GfVec3d(0.0, 1.0, 0.0)),
                              GfQuatd(0.0, GfVec3d(1.0, 0.0, 0.0))));
    arr.push_back(GfDualQuatd(GfQuatd(0.0, GfVec3d(1.0, 0.0, 0.0)),
                              GfQuatd(1.0, GfVec3d(0.0, 1.0, 0.0))));
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(desc.IsDualQuat());
    TF_AXIOM(!desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfDualQuatd>");
    TF_AXIOM(desc.ToString() == 
        "[((1, 0, 1, 0), (0, 1, 0, 0)), ((0, 1, 0, 0), (1, 0, 1, 0))]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 8);
    VtArray<GfDualQuatd> arr2 = desc.GetValue().Get<VtArray<GfDualQuatd>>();
    TF_AXIOM(arr2.size() == 2);
    TF_AXIOM(arr2[0] == GfDualQuatd(
        GfQuatd(1.0, GfVec3d(0.0, 1.0, 0.0)),
        GfQuatd(0.0, GfVec3d(1.0, 0.0, 0.0))));
    TF_AXIOM(arr2[1] == GfDualQuatd(
        GfQuatd(0.0, GfVec3d(1.0, 0.0, 0.0)),
        GfQuatd(1.0, GfVec3d(0.0, 1.0, 0.0))));
}

static void testArrayGfRange2f() {
    VtArray<GfRange2f> arr;
    arr.push_back(GfRange2f(GfVec2f(1.0f, 2.0f), GfVec2f(3.0f, 4.0f)));
    arr.push_back(GfRange2f(GfVec2f(5.0f, 6.0f), GfVec2f(7.0f, 8.0f)));
    VtValue v(arr);
    HydraPassthroughValueDescriptor desc(v);
    TF_AXIOM(desc.IsFloat());
    TF_AXIOM(!desc.IsInteger());
    TF_AXIOM(!desc.IsBool());
    TF_AXIOM(!desc.IsString());
    TF_AXIOM(desc.IsArray());
    TF_AXIOM(!desc.IsMatrix2());
    TF_AXIOM(!desc.IsMatrix3());
    TF_AXIOM(!desc.IsMatrix4());
    TF_AXIOM(!desc.IsVec2());
    TF_AXIOM(!desc.IsVec3());
    TF_AXIOM(!desc.IsVec4());
    TF_AXIOM(!desc.IsQuat());
    TF_AXIOM(!desc.IsDualQuat());
    TF_AXIOM(desc.IsRange2());
    TF_AXIOM(!desc.IsRange3());
    TF_AXIOM(desc.GetTypeName() == "VtArray<GfRange2f>");
    TF_AXIOM(desc.ToString() == "[[(1, 2)...(3, 4)], [(5, 6)...(7, 8)]]");
    TF_AXIOM(desc.GetArraySize() == 2);
    TF_AXIOM(desc.GetArrayItemDimension().size() == 1);
    TF_AXIOM(desc.GetArrayItemDimension()[0] == 2);
    VtArray<GfRange2f> arr2 = desc.GetValue().Get<VtArray<GfRange2f>>();
    TF_AXIOM(arr2.size() == 2);
    GfRange2f r1 = arr2[0];
    GfVec2f r1min = r1.GetMin();
    GfVec2f r1max = r1.GetMax();
    TF_AXIOM(std::abs(r1min[0] - 1.0f) < 1e-10f);
    TF_AXIOM(std::abs(r1min[1] - 2.0f) < 1e-10f);
    TF_AXIOM(std::abs(r1max[0] - 3.0f) < 1e-10f);
    TF_AXIOM(std::abs(r1max[1] - 4.0f) < 1e-10f);
    GfRange2f r2 = arr2[1];
    GfVec2f r2min = r2.GetMin();
    GfVec2f r2max = r2.GetMax();
    TF_AXIOM(std::abs(r2min[0] - 5.0f) < 1e-10f);
    TF_AXIOM(std::abs(r2min[1] - 6.0f) < 1e-10f);
    TF_AXIOM(std::abs(r2max[0] - 7.0f) < 1e-10f);
    TF_AXIOM(std::abs(r2max[1] - 8.0f) < 1e-10f);
}

int main(int argc, char *argv[])
{
    // Spot test a variety of types
    testFloat();
    testDouble();
    testHalf();
    testInt();
    testBool();
    testString();
    testToken();
    testGfVec2i();
    testGfVec2h();
    testGfVec4d();
    testGfMatrix2f();
    testGfMatrix4f();
    testGfMatrix3d();
    testGfQuatd();
    testGfDualQuatf();
    testGfRange3d();

    // Spot test a variety of array types
    testArrayInt();
    testArrayFloat();
    testArrayString();
    testArrayGfVec4i();
    testArrayGfVec3f();
    testArrayGfMatrix2d();
    testArrayGfMatrix4d();
    testArrayGfMatrix3f();
    testArrayGfQuatf();
    testArrayGfDualQuatd();
    testArrayGfRange2f();

    printf("Test SUCCEEDED\n");

    return 0;
}
